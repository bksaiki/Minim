/**
 * @file
 * Reader.
 */

#include "read.h"

#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "error.h"
#include "heap.h"
#include "minim.h"
#include "number.h"
#include "port.h"
#include "symbol.h"

// Characters of the token being read.  Static, so an error can't leak it.
static struct {
    uint32_t *chars;
    size_t len;
    size_t cap;
} token;

static void token_push(uint32_t c) {
    if (token.len == token.cap) {
        token.cap = token.cap ? 2 * token.cap : 64;
        token.chars = realloc(token.chars, token.cap * sizeof(*token.chars));
        if (token.chars == NULL)
            minim_out_of_memory();
    }
    token.chars[token.len++] = c;
}

static bool token_is(const char *s) {
    size_t n = strlen(s);
    if (token.len != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        if (token.chars[i] != (unsigned char)s[i])
            return false;
    }
    return true;
}

static minim_obj token_string(void) {
    minim_obj s = minim_make_string(token.len, 0);
    memcpy(minim_string_chars(s), token.chars, token.len * sizeof(uint32_t));
    return s;
}

// Raises the read error `msg` with irritant `x`.
static _Noreturn void read_error(const char *msg, minim_obj x) {
    minim_error_of_kind(MINIM_ERROR_READ, msg, x);
}

static bool is_whitespace(int32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static bool is_delimiter(int32_t c) {
    return c < 0 || is_whitespace(c) || c == '(' || c == ')' || c == '"' || c == ';';
}

// Where the reader reads from: a port, through a reference its caller roots,
// or a NUL-terminated string.
typedef struct source {
    const minim_obj *port;      // NULL for a string
    const unsigned char *str;   // next byte of the string
    const unsigned char *prev;  // start of the string's last code point read
} source;

// Reads one code point, or returns -1 at the end.
static int32_t next(source *in) {
    if (in->port != NULL)
        return minim_port_read_char(*in->port);
    in->prev = in->str;
    if (*in->str == '\0')
        return -1;
    uint32_t c;
    in->str += minim_utf8_decode(in->str, SIZE_MAX, &c);
    return (int32_t)c;
}

// Pushes back `c`, the last code point read, once.
static void unread(source *in, int32_t c) {
    if (in->port != NULL)
        minim_port_unread_char(*in->port, c);
    else
        in->str = in->prev;
}

// Appends characters to the token up to the next delimiter, which is left
// unread.
static void read_until_delimiter(source *in) {
    for (;;) {
        int32_t c = next(in);
        if (is_delimiter(c)) {
            unread(in, c);
            return;
        }
        token_push((uint32_t)c);
    }
}

// Skips whitespace and comments; returns the next character.
static int32_t skip_atmosphere(source *in) {
    for (;;) {
        int32_t c = next(in);
        if (c == ';') {
            do
                c = next(in);
            while (c >= 0 && c != '\n');
        } else if (!is_whitespace(c)) {
            return c;
        }
    }
}

static int hex_value(uint32_t c) {
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (int)(c - 'A' + 10);
    return -1;
}

// Parses token characters [start, end) as a hex scalar value, or returns -1.
static int32_t parse_hex(size_t start, size_t end) {
    if (start == end)
        return -1;
    uint32_t v = 0;
    for (size_t i = start; i < end; i++) {
        int d = hex_value(token.chars[i]);
        if (d < 0 || v > 0x10FFFF)
            return -1;
        v = 16 * v + (uint32_t)d;
    }
    return minim_is_scalar_value(v) ? (int32_t)v : -1;
}

// Parses the token as a number into `*x`; returns false if it isn't one.
static bool read_number(minim_obj *x) {
    switch (minim_parse_number(token.chars, token.len, 10, x)) {
    case MINIM_PARSE_OK:
        return true;
    case MINIM_PARSE_NOT_NUMBER:
        return false;
    case MINIM_PARSE_OUT_OF_RANGE:
        read_error("fixnum out of range", token_string());
    default:
        read_error("no exact representation", token_string());
    }
}

static bool is_intraline_whitespace(int32_t c) {
    return c == ' ' || c == '\t';
}

// Skips a line continuation, `\`, intraline white space, a line ending, and
// intraline white space, from `c`, its first code point after the `\`.
static void skip_continuation(source *in, int32_t c) {
    while (is_intraline_whitespace(c))
        c = next(in);
    bool cr = c == '\r';
    if (cr)
        c = next(in);
    if (c == '\n')
        c = next(in);
    else if (!cr)
        read_error("bad string escape", MINIM_VOID);
    while (is_intraline_whitespace(c))
        c = next(in);
    unread(in, c);
}

static minim_obj read_string(source *in) {
    token.len = 0;
    for (;;) {
        int32_t c = next(in);
        if (c < 0)
            read_error("eof in string", MINIM_VOID);
        if (c == '"')
            return token_string();
        if (c != '\\') {
            token_push((uint32_t)c);
            continue;
        }

        c = next(in);
        switch (c) {
        case '"': token_push('"'); break;
        case '\\': token_push('\\'); break;
        case '|': token_push('|'); break;
        case 'a': token_push('\a'); break;
        case 'b': token_push('\b'); break;
        case 'n': token_push('\n'); break;
        case 'r': token_push('\r'); break;
        case 't': token_push('\t'); break;
        case ' ':
        case '\t':
        case '\r':
        case '\n': skip_continuation(in, c); break;
        case 'x': {
            size_t start = token.len;
            while ((c = next(in)) >= 0 && c != ';')
                token_push((uint32_t)c);
            int32_t v = c == ';' ? parse_hex(start, token.len) : -1;
            if (v < 0)
                read_error("bad string escape", MINIM_VOID);
            token.len = start;
            token_push((uint32_t)v);
            break;
        }
        default:
            read_error("bad string escape", MINIM_VOID);
        }
    }
}

static minim_obj read_char(source *in) {
    // The first character is part of the token even if it is a delimiter.
    int32_t c = next(in);
    if (c < 0)
        read_error("eof in char", MINIM_VOID);
    token.len = 0;
    token_push((uint32_t)c);
    read_until_delimiter(in);

    if (token.len == 1)
        return minim_char((uint32_t)c);
    for (const minim_char_name *n = minim_char_names; n->name != NULL; n++) {
        if (token_is(n->name))
            return minim_char(n->c);
    }
    int32_t v = c == 'x' ? parse_hex(1, token.len) : -1;
    if (v < 0)
        read_error("bad char", token_string());
    return minim_char((uint32_t)v);
}

// What read_item found
enum { ITEM, CLOSE, DOT, END };

static int read_item(source *in, minim_obj *x);
static int read_item_from(source *in, int32_t c, minim_obj *x);

// Reads a datum, which must be present.
static minim_obj read_required(source *in, const char *context) {
    minim_obj x;
    if (read_item(in, &x) != ITEM)
        read_error(context, MINIM_VOID);
    return x;
}

// Reads the rest of a list after its `(`.
static minim_obj read_list(source *in) {
    minim_obj head = MINIM_NIL;
    minim_obj tail = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&head, &tail, &x);
    for (;;) {
        switch (read_item(in, &x)) {
        case END:
            read_error("eof in list", MINIM_VOID);
        case CLOSE:
            MINIM_RETURN(head);
        case DOT:
            if (minim_is_null(head))
                read_error("bad dotted list", MINIM_VOID);
            x = read_required(in, "bad dotted list");
            minim_set_cdr(tail, x);
            if (read_item(in, &x) != CLOSE)
                read_error("bad dotted list", MINIM_VOID);
            MINIM_RETURN(head);
        default: {
            minim_obj pair = minim_cons(x, MINIM_NIL);
            if (minim_is_null(head))
                head = pair;
            else
                minim_set_cdr(tail, pair);
            tail = pair;
        }
        }
    }
}

// Reads the rest of a vector after its `#(`.
static minim_obj read_vector(source *in) {
    minim_obj lst = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&lst, &x);
    size_t n = 0;
    int kind;
    while ((kind = read_item(in, &x)) == ITEM) {
        lst = minim_cons(x, lst);
        n++;
    }
    if (kind != CLOSE)
        read_error(kind == END ? "eof in vector" : "bad vector", MINIM_VOID);

    minim_obj v = minim_make_vector(n, MINIM_NIL);
    for (; n > 0; n--, lst = minim_cdr(lst))
        minim_vector_set(v, n - 1, minim_car(lst));
    MINIM_RETURN(v);
}

// Reads the rest of a bytevector after its `#u8(`: bytes, fixnums 0 to 255.
static minim_obj read_bytevector(source *in) {
    minim_obj lst = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&lst, &x);
    size_t n = 0;
    int kind;
    while ((kind = read_item(in, &x)) == ITEM) {
        if (!minim_is_fixnum(x) || minim_fixnum_value(x) < 0 || minim_fixnum_value(x) > 255)
            read_error("bad byte", x);
        lst = minim_cons(x, lst);
        n++;
    }
    if (kind != CLOSE)
        read_error(kind == END ? "eof in bytevector" : "bad bytevector", MINIM_VOID);

    minim_obj bv = minim_make_bytevector(n, 0);
    uint8_t *bytes = minim_bytevector_data(bv);
    for (; n > 0; n--, lst = minim_cdr(lst))
        bytes[n - 1] = (uint8_t)minim_fixnum_value(minim_car(lst));
    MINIM_RETURN(bv);
}

// Skips a block comment, after its `#|`, to the matching `|#`: they nest.
static void skip_block_comment(source *in) {
    int32_t prev = 0;
    for (int depth = 1; depth > 0;) {
        int32_t c = next(in);
        if (c < 0)
            read_error("eof in comment", MINIM_VOID);
        if (prev == '|' && c == '#') {
            depth--;
            c = 0;  // a delimiter's chars don't start another
        } else if (prev == '#' && c == '|') {
            depth++;
            c = 0;
        }
        prev = c;
    }
}

// Reads the datum after `#c`, `c` read.
static minim_obj read_hash(source *in, int32_t c) {
    if (c == '(')
        return read_vector(in);
    if (c == '\\')
        return read_char(in);

    unread(in, c);
    token.len = 0;
    token_push('#');
    read_until_delimiter(in);
    if (token_is("#t") || token_is("#true"))
        return MINIM_TRUE;
    if (token_is("#f") || token_is("#false"))
        return MINIM_FALSE;
    if (token_is("#u8") && next(in) == '(')
        return read_bytevector(in);
    minim_obj x;
    if (read_number(&x))
        return x;
    read_error("bad # syntax", token_string());
}

// Reads the datum after an abbreviation like `'`, starting with `c`, and
// returns `(*sym datum)`.  `*sym` is loaded after the read, which may collect.
static minim_obj read_abbreviation(source *in, int32_t c, const minim_obj *sym, const char *msg) {
    minim_obj datum;
    if (read_item_from(in, c, &datum) != ITEM)
        read_error(msg, MINIM_VOID);
    datum = minim_cons(datum, MINIM_NIL);
    return minim_cons(*sym, datum);
}

// Reads the next item into `*x`; returns what was found.
static int read_item(source *in, minim_obj *x) {
    return read_item_from(in, skip_atmosphere(in), x);
}

// Like read_item, with `c` the item's first character, already read.
static int read_item_from(source *in, int32_t c, minim_obj *x) {
    switch (c) {
    case -1:
        return END;
    case ')':
        return CLOSE;
    case '(':
        *x = read_list(in);
        return ITEM;
    case '"':
        *x = read_string(in);
        return ITEM;
    case '#':
        c = next(in);
        if (c == '|' || c == ';') {  // a block or datum comment, then the next item
            if (c == '|')
                skip_block_comment(in);
            else
                read_required(in, "bad datum comment");
            return read_item(in, x);
        }
        *x = read_hash(in, c);
        return ITEM;
    case '\'':
        *x = read_abbreviation(in, skip_atmosphere(in), &minim_sym_quote, "eof after quote");
        return ITEM;
    case '`':
        *x = read_abbreviation(in, skip_atmosphere(in), &minim_sym_quasiquote,
                               "eof after quasiquote");
        return ITEM;
    case ',':
        // `,@` is one token; otherwise the next character may begin the datum.
        c = next(in);
        if (c == '@') {
            *x = read_abbreviation(in, skip_atmosphere(in), &minim_sym_unquote_splicing,
                                   "eof after unquote-splicing");
            return ITEM;
        }
        if (is_whitespace(c) || c == ';') {
            unread(in, c);
            c = skip_atmosphere(in);
        }
        *x = read_abbreviation(in, c, &minim_sym_unquote, "eof after unquote");
        return ITEM;
    default:
        token.len = 0;
        token_push((uint32_t)c);
        read_until_delimiter(in);
        if (token_is("."))
            return DOT;
        if (!read_number(x))
            *x = minim_intern(token_string());
        return ITEM;
    }
}

static minim_obj read_datum(source *in) {
    minim_obj x;
    switch (read_item(in, &x)) {
    case END:
        return MINIM_EOF;
    case CLOSE:
        read_error("unexpected )", MINIM_VOID);
    case DOT:
        read_error("unexpected .", MINIM_VOID);
    default:
        return x;
    }
}

minim_obj minim_read(minim_obj port) {
    MINIM_FRAME(&port);
    source src = {.port = &port};
    MINIM_RETURN(read_datum(&src));
}

minim_obj minim_read_string(const char **s) {
    source src = {.str = (const unsigned char *)*s};
    minim_obj x = read_datum(&src);
    *s = (const char *)src.str;
    return x;
}

// Reads every form from the port at `port` into a list.
static minim_obj read_forms(void *port) {
    minim_obj head = MINIM_NIL;
    minim_obj tail = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&head, &tail, &x);
    while ((x = minim_read(*(minim_obj *)port)) != MINIM_EOF) {
        x = minim_cons(x, MINIM_NIL);
        if (minim_is_null(head))
            head = x;
        else
            minim_set_cdr(tail, x);
        tail = x;
    }
    MINIM_RETURN(head);
}

minim_obj minim_read_file(minim_obj path) {
    minim_obj port = minim_port_open_file(path, MINIM_PORT_INPUT, "load: cannot open file");
    MINIM_FRAME(&port);
    minim_obj forms;
    bool ok = minim_catch(read_forms, &port, &forms);
    minim_port_close(port);
    if (!ok)
        minim_reraise();
    MINIM_RETURN(forms);
}

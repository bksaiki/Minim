/**
 * @file
 * Printer.
 */

#include <inttypes.h>
#include <stdlib.h>

#include "char.h"
#include "heap.h"
#include "minim.h"
#include "port.h"
#include "symbol.h"

// Where the printer writes: a file, or a buffer of chars, malloc'd, so that
// printing never allocates on the heap.
typedef struct sink {
    FILE *file;  // NULL for the buffer
    uint32_t *chars;
    size_t len;
    size_t cap;
} sink;

static void put(sink *out, uint32_t c) {
    if (out->file != NULL) {
        unsigned char buf[4];
        fwrite(buf, 1, minim_utf8_encode(c, buf), out->file);
        return;
    }
    if (out->len == out->cap) {
        out->cap = out->cap ? 2 * out->cap : 64;
        out->chars = realloc(out->chars, out->cap * sizeof(uint32_t));
        if (out->chars == NULL)
            minim_out_of_memory();
    }
    out->chars[out->len++] = c;
}

static void put_ascii(sink *out, const char *s) {
    if (out->file != NULL) {
        fputs(s, out->file);
        return;
    }
    for (; *s != '\0'; s++)
        put(out, (unsigned char)*s);
}

// Control characters print as hex escapes.
static bool is_control(uint32_t c) {
    return c < 0x20 || (c >= 0x7F && c < 0xA0);
}

static void write_char(sink *out, uint32_t c) {
    put_ascii(out, "#\\");
    switch (c) {
    case 0x00: put_ascii(out, "nul"); break;
    case '\t': put_ascii(out, "tab"); break;
    case '\n': put_ascii(out, "newline"); break;
    case ' ': put_ascii(out, "space"); break;
    default:
        if (is_control(c)) {
            char buf[16];
            snprintf(buf, sizeof(buf), "x%" PRIx32, c);
            put_ascii(out, buf);
        }
        else
            put(out, c);
    }
}

// Writes the characters of the string `s` unescaped.
static void write_chars(sink *out, minim_obj s) {
    const uint32_t *chars = minim_string_chars(s);
    for (size_t i = 0; i < minim_string_length(s); i++)
        put(out, chars[i]);
}

static void write_string(sink *out, minim_obj s) {
    const uint32_t *chars = minim_string_chars(s);
    size_t n = minim_string_length(s);
    put(out, '"');
    for (size_t i = 0; i < n; i++) {
        uint32_t c = chars[i];
        switch (c) {
        case '"': put_ascii(out, "\\\""); break;
        case '\\': put_ascii(out, "\\\\"); break;
        case '\n': put_ascii(out, "\\n"); break;
        case '\t': put_ascii(out, "\\t"); break;
        default:
            if (is_control(c)) {
                char buf[16];
                snprintf(buf, sizeof(buf), "\\x%" PRIx32 ";", c);
                put_ascii(out, buf);
            }
            else
                put(out, c);
        }
    }
    put(out, '"');
}

// The abbreviation for the pair `x` if it is `(quote d)`, `(quasiquote d)`,
// `(unquote d)`, or `(unquote-splicing d)`; otherwise NULL.
static const char *abbreviation(minim_obj x) {
    minim_obj rest = minim_cdr(x);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        return NULL;
    minim_obj head = minim_car(x);
    minim_obj d = minim_car(rest);
    if (head == minim_sym_quote)
        return "'";
    if (head == minim_sym_quasiquote)
        return "`";
    if (head == minim_sym_unquote_splicing)
        return ",@";
    // `(unquote @x)` stays long: `,@x` reads as unquote-splicing.
    if (head == minim_sym_unquote
        && !(minim_is_symbol(d) && minim_string_length(minim_symbol_name(d)) > 0
             && minim_string_ref(minim_symbol_name(d), 0) == '@'))
        return ",";
    return NULL;
}

static void print(sink *out, minim_obj x, bool display);

static void write_pair(sink *out, minim_obj x, bool display) {
    const char *abbrev = abbreviation(x);
    if (abbrev != NULL) {
        put_ascii(out, abbrev);
        print(out, minim_car(minim_cdr(x)), display);
        return;
    }

    put(out, '(');
    print(out, minim_car(x), display);
    // A tail like `(unquote x)` prints dotted and abbreviated: `(a . ,x)`.
    for (x = minim_cdr(x); minim_is_pair(x) && abbreviation(x) == NULL; x = minim_cdr(x)) {
        put(out, ' ');
        print(out, minim_car(x), display);
    }
    if (!minim_is_null(x)) {
        put_ascii(out, " . ");
        print(out, x, display);
    }
    put(out, ')');
}

static void write_vector(sink *out, minim_obj v, bool display) {
    put_ascii(out, "#(");
    for (size_t i = 0; i < minim_vector_length(v); i++) {
        if (i > 0)
            put(out, ' ');
        print(out, minim_vector_ref(v, i), display);
    }
    put(out, ')');
}

// Writes `#<procedure name>`, or `#<procedure>` if `name` is #f.
static void write_procedure(sink *out, minim_obj name) {
    put_ascii(out, "#<procedure");
    if (name != MINIM_FALSE) {
        put(out, ' ');
        write_chars(out, minim_symbol_name(name));
    }
    put(out, '>');
}

// Writes the record `r` as `#<name>`, or an rtd as `#<record-type name>`, with
// one pair of angle brackets stripped from `name`.
static void write_record(sink *out, minim_obj r) {
    minim_obj rtd = minim_record_rtd(r);
    if (rtd == minim_base_rtd) {
        put_ascii(out, "#<record-type ");
        rtd = r;
    } else {
        put_ascii(out, "#<");
    }
    minim_obj name = minim_symbol_name(minim_record_ref(rtd, MINIM_RTD_NAME));
    const uint32_t *chars = minim_string_chars(name);
    size_t n = minim_string_length(name);
    bool strip = n >= 2 && chars[0] == '<' && chars[n - 1] == '>';
    for (size_t i = strip; i < n - strip; i++)
        put(out, chars[i]);
    put(out, '>');
}

// Prints `x` in `write` syntax, or with strings and chars unescaped if
// `display`.
static void print(sink *out, minim_obj x, bool display) {
    if (minim_is_fixnum(x)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%" PRIdPTR, minim_fixnum_value(x));
        put_ascii(out, buf);
    }
    else if (minim_is_char(x) && display)
        put(out, minim_char_value(x));
    else if (minim_is_char(x))
        write_char(out, minim_char_value(x));
    else if (minim_is_pair(x))
        write_pair(out, x, display);
    else if (minim_is_symbol(x))
        write_chars(out, minim_symbol_name(x));
    else if (minim_is_vector(x))
        write_vector(out, x, display);
    else if (minim_is_string(x) && display)
        write_chars(out, x);
    else if (minim_is_string(x))
        write_string(out, x);
    else if (minim_is_closure(x))
        write_procedure(out, minim_closure_name(x));
    else if (minim_is_continuation(x))
        put_ascii(out, "#<continuation>");
    else if (minim_is_port(x)) {
        put_ascii(out, minim_port_flags(x) & MINIM_PORT_INPUT ? "#<input port " : "#<output port ");
        write_chars(out, minim_port_name(x));
        put(out, '>');
    }
    else if (minim_is_record(x))
        write_record(out, x);
    else if (minim_is_error_object(x)) {
        put_ascii(out, "#<error ");
        print(out, minim_error_object_message(x), true);
        put(out, '>');
    }
    else if (minim_is_primitive(x))
        write_procedure(out, minim_primitive_name(x));
    else if (x == MINIM_TRUE)
        put_ascii(out, "#t");
    else if (x == MINIM_FALSE)
        put_ascii(out, "#f");
    else if (x == MINIM_NIL)
        put_ascii(out, "()");
    else if (x == MINIM_VOID)
        put_ascii(out, "#<void>");
    else if (x == MINIM_EOF)
        put_ascii(out, "#<eof>");
    else if (x == MINIM_UNBOUND)
        put_ascii(out, "#<unbound>");
    else
        put_ascii(out, "#<garbage>");
}

void minim_write(FILE *out, minim_obj x) {
    sink s = {.file = out};
    print(&s, x, false);
}

// A string port gets the chars after the walk, the only step that allocates.
static void print_to_port(minim_obj port, minim_obj x, bool display) {
    if (minim_port_flags(port) & MINIM_PORT_FILE) {
        sink s = {.file = minim_port_file(port)};
        print(&s, x, display);
        return;
    }
    sink s = {0};
    print(&s, x, display);
    minim_port_write_chars(port, s.chars, s.len);
    free(s.chars);
}

void minim_port_write(minim_obj port, minim_obj x) {
    print_to_port(port, x, false);
}

void minim_port_display(minim_obj port, minim_obj x) {
    print_to_port(port, x, true);
}

/**
 * @file
 * Printer.
 */

#include <inttypes.h>

#include "minim.h"
#include "symbol.h"
#include "char.h"

static void write_utf8(FILE *out, uint32_t c) {
    unsigned char buf[4];
    fwrite(buf, 1, minim_utf8_encode(c, buf), out);
}

// Control characters print as hex escapes.
static bool is_control(uint32_t c) {
    return c < 0x20 || (c >= 0x7F && c < 0xA0);
}

static void write_char(FILE *out, uint32_t c) {
    fputs("#\\", out);
    switch (c) {
    case 0x00: fputs("nul", out); break;
    case '\t': fputs("tab", out); break;
    case '\n': fputs("newline", out); break;
    case ' ': fputs("space", out); break;
    default:
        if (is_control(c))
            fprintf(out, "x%" PRIx32, c);
        else
            write_utf8(out, c);
    }
}

// Writes the characters of the string `s` unescaped.
static void write_chars(FILE *out, minim_obj s) {
    const uint32_t *chars = minim_string_chars(s);
    for (size_t i = 0; i < minim_string_length(s); i++)
        write_utf8(out, chars[i]);
}

static void write_string(FILE *out, minim_obj s) {
    const uint32_t *chars = minim_string_chars(s);
    size_t n = minim_string_length(s);
    fputc('"', out);
    for (size_t i = 0; i < n; i++) {
        uint32_t c = chars[i];
        switch (c) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (is_control(c))
                fprintf(out, "\\x%" PRIx32 ";", c);
            else
                write_utf8(out, c);
        }
    }
    fputc('"', out);
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

static void print(FILE *out, minim_obj x, bool display);

static void write_pair(FILE *out, minim_obj x, bool display) {
    const char *abbrev = abbreviation(x);
    if (abbrev != NULL) {
        fputs(abbrev, out);
        print(out, minim_car(minim_cdr(x)), display);
        return;
    }

    fputc('(', out);
    print(out, minim_car(x), display);
    // A tail like `(unquote x)` prints dotted and abbreviated: `(a . ,x)`.
    for (x = minim_cdr(x); minim_is_pair(x) && abbreviation(x) == NULL; x = minim_cdr(x)) {
        fputc(' ', out);
        print(out, minim_car(x), display);
    }
    if (!minim_is_null(x)) {
        fputs(" . ", out);
        print(out, x, display);
    }
    fputc(')', out);
}

static void write_vector(FILE *out, minim_obj v, bool display) {
    fputs("#(", out);
    for (size_t i = 0; i < minim_vector_length(v); i++) {
        if (i > 0)
            fputc(' ', out);
        print(out, minim_vector_ref(v, i), display);
    }
    fputc(')', out);
}

// Prints `x` in `write` syntax, or with strings and chars unescaped if
// `display`.
static void print(FILE *out, minim_obj x, bool display) {
    if (minim_is_fixnum(x))
        fprintf(out, "%" PRIdPTR, minim_fixnum_value(x));
    else if (minim_is_char(x) && display)
        write_utf8(out, minim_char_value(x));
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
        fputs("#<procedure>", out);
    else if (minim_is_continuation(x))
        fputs("#<continuation>", out);
    else if (minim_is_error_object(x)) {
        fputs("#<error ", out);
        print(out, minim_error_object_message(x), true);
        fputc('>', out);
    }
    else if (minim_is_primitive(x)) {
        fputs("#<procedure ", out);
        write_chars(out, minim_symbol_name(minim_primitive_name(x)));
        fputc('>', out);
    }
    else if (x == MINIM_TRUE)
        fputs("#t", out);
    else if (x == MINIM_FALSE)
        fputs("#f", out);
    else if (x == MINIM_NIL)
        fputs("()", out);
    else if (x == MINIM_VOID)
        fputs("#<void>", out);
    else if (x == MINIM_EOF)
        fputs("#<eof>", out);
    else if (x == MINIM_UNBOUND)
        fputs("#<unbound>", out);
    else
        fputs("#<garbage>", out);
}

void minim_write(FILE *out, minim_obj x) {
    print(out, x, false);
}

void minim_display(FILE *out, minim_obj x) {
    print(out, x, true);
}

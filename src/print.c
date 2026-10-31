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

static void write_symbol(FILE *out, minim_obj sym) {
    minim_obj name = minim_symbol_name(sym);
    const uint32_t *chars = minim_string_chars(name);
    for (size_t i = 0; i < minim_string_length(name); i++)
        write_utf8(out, chars[i]);
}

// Whether `x` is `(quote y)`.
static bool is_quote_form(minim_obj x) {
    minim_obj rest = minim_cdr(x);
    return minim_car(x) == minim_sym_quote && minim_is_pair(rest)
        && minim_is_null(minim_cdr(rest));
}

static void write_pair(FILE *out, minim_obj x) {
    if (is_quote_form(x)) {
        fputc('\'', out);
        minim_write(out, minim_car(minim_cdr(x)));
        return;
    }

    fputc('(', out);
    minim_write(out, minim_car(x));
    for (x = minim_cdr(x); minim_is_pair(x); x = minim_cdr(x)) {
        fputc(' ', out);
        minim_write(out, minim_car(x));
    }
    if (!minim_is_null(x)) {
        fputs(" . ", out);
        minim_write(out, x);
    }
    fputc(')', out);
}

static void write_vector(FILE *out, minim_obj v) {
    fputs("#(", out);
    for (size_t i = 0; i < minim_vector_length(v); i++) {
        if (i > 0)
            fputc(' ', out);
        minim_write(out, minim_vector_ref(v, i));
    }
    fputc(')', out);
}

void minim_write(FILE *out, minim_obj x) {
    if (minim_is_fixnum(x))
        fprintf(out, "%" PRIdPTR, minim_fixnum_value(x));
    else if (minim_is_char(x))
        write_char(out, minim_char_value(x));
    else if (minim_is_pair(x))
        write_pair(out, x);
    else if (minim_is_symbol(x))
        write_symbol(out, x);
    else if (minim_is_vector(x))
        write_vector(out, x);
    else if (minim_is_string(x))
        write_string(out, x);
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

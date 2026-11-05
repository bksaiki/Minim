/**
 * @file
 * Characters: UTF-8 encoding and decoding, and the char primitives.
 */

#include "char.h"

#include <stdlib.h>

#include "heap.h"
#include "minim.h"
#include "prims.h"

size_t minim_utf8_decode(const unsigned char *s, uint32_t *c) {
    size_t len;
    uint32_t cp;
    uint32_t min;  // smallest code point that needs `len` bytes
    if (s[0] < 0x80) {
        *c = s[0];
        return 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        len = 2;
        cp = s[0] & 0x1F;
        min = 0x80;
    } else if ((s[0] & 0xF0) == 0xE0) {
        len = 3;
        cp = s[0] & 0x0F;
        min = 0x800;
    } else if ((s[0] & 0xF8) == 0xF0) {
        len = 4;
        cp = s[0] & 0x07;
        min = 0x10000;
    } else {
        *c = 0xFFFD;
        return 1;
    }

    // Stops at the terminating NUL, which isn't a continuation byte.
    for (size_t i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *c = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (cp < min || !minim_is_scalar_value(cp)) {
        *c = 0xFFFD;
        return 1;
    }
    *c = cp;
    return len;
}

int32_t minim_utf8_getc(FILE *in) {
    int b = getc(in);
    if (b == EOF)
        return -1;

    size_t len = 1;
    if ((b & 0xE0) == 0xC0)
        len = 2;
    else if ((b & 0xF0) == 0xE0)
        len = 3;
    else if ((b & 0xF8) == 0xF0)
        len = 4;

    // Read continuation bytes, leaving the first non-continuation byte.
    unsigned char buf[5] = {(unsigned char)b};
    size_t n = 1;
    while (n < len) {
        int c = getc(in);
        if (c == EOF)
            break;
        if ((c & 0xC0) != 0x80) {
            ungetc(c, in);
            break;
        }
        buf[n++] = (unsigned char)c;
    }
    buf[n] = '\0';

    uint32_t c;
    if (minim_utf8_decode(buf, &c) != n)
        c = 0xFFFD;
    return (int32_t)c;
}

size_t minim_utf8_encode(uint32_t c, unsigned char out[4]) {
    if (c < 0x80) {
        out[0] = (unsigned char)c;
        return 1;
    } else if (c < 0x800) {
        out[0] = (unsigned char)(0xC0 | (c >> 6));
        out[1] = (unsigned char)(0x80 | (c & 0x3F));
        return 2;
    } else if (c < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (c >> 12));
        out[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (c & 0x3F));
        return 3;
    } else {
        out[0] = (unsigned char)(0xF0 | (c >> 18));
        out[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (unsigned char)(0x80 | (c & 0x3F));
        return 4;
    }
}

char *minim_string_to_utf8(minim_obj s) {
    size_t n = minim_string_length(s);
    unsigned char *out = malloc(4 * n + 1);
    if (out == NULL)
        minim_out_of_memory();
    size_t len = 0;
    for (size_t i = 0; i < n; i++)
        len += minim_utf8_encode(minim_string_ref(s, i), out + len);
    out[len] = '\0';
    return (char *)out;
}

const minim_char_name minim_char_names[] = {
    {"alarm", 0x07},   {"backspace", 0x08}, {"delete", 0x7F}, {"escape", 0x1B},
    {"newline", '\n'}, {"null", 0x00},      {"return", '\r'}, {"space", ' '},
    {"tab", '\t'},     {"nul", 0x00},       {NULL, 0},
};

static minim_obj prim_is_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_char(args[0]));
}

static minim_obj prim_char_to_integer(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return minim_fixnum(char_arg(args[0], "char->integer: not a char"));
}

static minim_obj prim_integer_to_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t v = fixnum_arg(args[0], "integer->char: not a fixnum");
    check(v >= 0 && v <= 0x10FFFF && minim_is_scalar_value((uint32_t)v),
          "integer->char: not a scalar value", args[0]);
    return minim_char((uint32_t)v);
}

static minim_obj prim_char_eq(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, eq, "char=?: not a char");
}

static minim_obj prim_char_lt(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, lt, "char<?: not a char");
}

static minim_obj prim_char_gt(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, gt, "char>?: not a char");
}

static minim_obj prim_char_le(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, le, "char<=?: not a char");
}

static minim_obj prim_char_ge(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, ge, "char>=?: not a char");
}

const minim_prim_def minim_char_prims[] = {
    {"char?", prim_is_char, 1, 1},
    {"char->integer", prim_char_to_integer, 1, 1},
    {"integer->char", prim_integer_to_char, 1, 1},
    {"char=?", prim_char_eq, 1, MINIM_VARIADIC},
    {"char<?", prim_char_lt, 1, MINIM_VARIADIC},
    {"char>?", prim_char_gt, 1, MINIM_VARIADIC},
    {"char<=?", prim_char_le, 1, MINIM_VARIADIC},
    {"char>=?", prim_char_ge, 1, MINIM_VARIADIC},
    {NULL, NULL, 0, 0},
};

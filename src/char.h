/**
 * @file
 * Characters: UTF-8 encoding and decoding.
 */

#ifndef MINIM_CHAR_H
#define MINIM_CHAR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "minim.h"

/**
 * Decodes one code point from the `n` bytes, at least one, at `s` into `*c`;
 * returns the bytes consumed.  An invalid sequence decodes to U+FFFD and
 * consumes one byte.  A NUL ends a sequence too, so a NUL-terminated `s` may
 * pass SIZE_MAX.
 */
size_t minim_utf8_decode(const unsigned char *s, size_t n, uint32_t *c);

/**
 * Reads one code point from `in`, or returns -1 at end of file.  An invalid
 * sequence reads as U+FFFD.
 */
int32_t minim_utf8_getc(FILE *in);

/** Encodes the scalar value `c` into `out`; returns the byte count. */
size_t minim_utf8_encode(uint32_t c, unsigned char out[4]);

/** The string `s` as a `malloc`'d, NUL-terminated UTF-8 C string. */
char *minim_string_to_utf8(minim_obj s);

/** A char's name, as in `#\name`. */
typedef struct minim_char_name {
    const char *name;
    uint32_t c;
} minim_char_name;

/**
 * R7RS's char names, then Chez's `nul`, ending with a NULL name; a char is
 * written with its first.
 */
extern const minim_char_name minim_char_names[];

#endif

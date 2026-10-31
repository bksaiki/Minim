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
 * Decodes one code point from the NUL-terminated `s` into `*c`; returns the
 * bytes consumed.  An invalid sequence decodes to U+FFFD and consumes one byte.
 */
size_t minim_utf8_decode(const unsigned char *s, uint32_t *c);

/**
 * Reads one code point from `in`, or returns -1 at end of file.  An invalid
 * sequence reads as U+FFFD.
 */
int32_t minim_utf8_getc(FILE *in);

/** Encodes the scalar value `c` into `out`; returns the byte count. */
size_t minim_utf8_encode(uint32_t c, unsigned char out[4]);

/** The string `s` as a `malloc`'d, NUL-terminated UTF-8 C string. */
char *minim_string_to_utf8(minim_obj s);

#endif

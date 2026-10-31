/**
 * @file
 * Characters: UTF-8 encoding and decoding.
 */

#ifndef MINIM_CHAR_H
#define MINIM_CHAR_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decodes one code point from the NUL-terminated `s` into `*c`; returns the
 * bytes consumed.  An invalid sequence decodes to U+FFFD and consumes one byte.
 */
size_t minim_utf8_decode(const unsigned char *s, uint32_t *c);

/** Encodes the scalar value `c` into `out`; returns the byte count. */
size_t minim_utf8_encode(uint32_t c, unsigned char out[4]);

#endif

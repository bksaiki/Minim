/**
 * @file
 * Unicode properties and case mappings of code points.
 */

#ifndef MINIM_UNICODE_H
#define MINIM_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Properties of a code point, as bits. */
enum {
    MINIM_UNICODE_ALPHABETIC = 1 << 0,      ///< `Alphabetic`
    MINIM_UNICODE_UPPER = 1 << 1,           ///< `Uppercase`
    MINIM_UNICODE_LOWER = 1 << 2,           ///< `Lowercase`
    MINIM_UNICODE_SPACE = 1 << 3,           ///< `White_Space`
    MINIM_UNICODE_DIGIT = 1 << 4,           ///< a decimal digit: general category `Nd`
    MINIM_UNICODE_CASED = 1 << 5,           ///< `Cased`
    MINIM_UNICODE_CASE_IGNORABLE = 1 << 6,  ///< `Case_Ignorable`
};

/** Most code points in a full case mapping. */
enum { MINIM_UNICODE_MAX_MAPPING = 3 };

/** Properties of the code point `c`. */
unsigned minim_unicode_props(uint32_t c);

/** Value of the decimal digit `c`, or -1 if `c` isn't one. */
int minim_unicode_digit(uint32_t c);

/** Simple uppercase mapping of `c`. */
uint32_t minim_unicode_upcase(uint32_t c);

/** Simple lowercase mapping of `c`. */
uint32_t minim_unicode_downcase(uint32_t c);

/** Simple case folding of `c`. */
uint32_t minim_unicode_foldcase(uint32_t c);

/**
 * Full uppercase mapping of `s[i]`, of the `n` code points `s`, into `out`;
 * returns its length.
 */
size_t minim_unicode_full_upcase(const uint32_t *s, size_t n, size_t i,
                                 uint32_t out[MINIM_UNICODE_MAX_MAPPING]);

/**
 * Full lowercase mapping of `s[i]`, of the `n` code points `s`, into `out`;
 * returns its length.  A capital sigma ending a word becomes a final sigma.
 */
size_t minim_unicode_full_downcase(const uint32_t *s, size_t n, size_t i,
                                   uint32_t out[MINIM_UNICODE_MAX_MAPPING]);

/**
 * Full case folding of `s[i]`, of the `n` code points `s`, into `out`;
 * returns its length.
 */
size_t minim_unicode_full_foldcase(const uint32_t *s, size_t n, size_t i,
                                   uint32_t out[MINIM_UNICODE_MAX_MAPPING]);

#endif

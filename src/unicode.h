/**
 * @file
 * Unicode properties and simple case mappings of code points.
 */

#ifndef MINIM_UNICODE_H
#define MINIM_UNICODE_H

#include <stdint.h>

/** Properties of a code point, as bits. */
enum {
    MINIM_UNICODE_ALPHABETIC = 1 << 0,  ///< `Alphabetic`
    MINIM_UNICODE_UPPER = 1 << 1,       ///< `Uppercase`
    MINIM_UNICODE_LOWER = 1 << 2,       ///< `Lowercase`
    MINIM_UNICODE_SPACE = 1 << 3,       ///< `White_Space`
    MINIM_UNICODE_DIGIT = 1 << 4,       ///< a decimal digit: general category `Nd`
};

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

#endif

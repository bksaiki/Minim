/**
 * @file
 * Parsing numbers, for the reader and `string->number`.
 */

#ifndef MINIM_NUMBER_H
#define MINIM_NUMBER_H

#include "minim.h"

/** What minim_parse_number found. */
typedef enum minim_parse {
    MINIM_PARSE_OK,            ///< a number, in `*x`
    MINIM_PARSE_NOT_NUMBER,    ///< not a number's syntax
    MINIM_PARSE_OUT_OF_RANGE,  ///< an exact integer beyond the fixnums
    MINIM_PARSE_NOT_EXACT,     ///< `#e` of a value with no exact representation
} minim_parse;

/**
 * Parses the `n` chars `s` as a number in `radix` (2, 8, 10, or 16), unless a
 * prefix gives another, into `*x`.  `s` may be in the heap: it is read before
 * anything is allocated.
 */
minim_parse minim_parse_number(const uint32_t *s, size_t n, unsigned radix, minim_obj *x);

#endif

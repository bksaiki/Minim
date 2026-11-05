/**
 * @file
 * Unicode properties and case mappings, from generated tables.
 */

#include "unicode.h"

#include <assert.h>
#include <stdlib.h>

typedef struct prop_range {
    uint32_t first;
    uint32_t last;
    unsigned char props;
} prop_range;

typedef struct case_row {
    uint32_t c;
    uint32_t upper;
    uint32_t lower;
    uint32_t fold;
} case_row;

typedef struct full_row {
    uint32_t c;
    uint32_t upper[MINIM_UNICODE_MAX_MAPPING];
    uint32_t lower[MINIM_UNICODE_MAX_MAPPING];
    uint32_t fold[MINIM_UNICODE_MAX_MAPPING];
} full_row;

#include "unicode_data.inc"

static int compare_range(const void *key, const void *elt) {
    uint32_t c = *(const uint32_t *)key;
    const prop_range *r = elt;
    return c < r->first ? -1 : c > r->last;
}

// Compares the code point `key` with the first field of a row, its code point.
static int compare_row(const void *key, const void *elt) {
    uint32_t c = *(const uint32_t *)key;
    uint32_t r = *(const uint32_t *)elt;
    return c < r ? -1 : c > r;
}

// The range of `props` holding `c`, or NULL.
static const prop_range *find_range(uint32_t c) {
    return bsearch(&c, props, sizeof(props) / sizeof(props[0]), sizeof(props[0]), compare_range);
}

// The row of `cases` for `c`, or NULL.
static const case_row *find_row(uint32_t c) {
    return bsearch(&c, cases, sizeof(cases) / sizeof(cases[0]), sizeof(cases[0]), compare_row);
}

// The row of `fulls` for `c`, or NULL.
static const full_row *find_full(uint32_t c) {
    return bsearch(&c, fulls, sizeof(fulls) / sizeof(fulls[0]), sizeof(fulls[0]), compare_row);
}

unsigned minim_unicode_props(uint32_t c) {
    const prop_range *r = find_range(c);
    return r == NULL ? 0 : r->props;
}

// Digits come in runs of ten from 0, which the generator checks.
int minim_unicode_digit(uint32_t c) {
    const prop_range *r = find_range(c);
    return r != NULL && (r->props & MINIM_UNICODE_DIGIT) ? (int)((c - r->first) % 10) : -1;
}

uint32_t minim_unicode_upcase(uint32_t c) {
    const case_row *r = find_row(c);
    return r == NULL ? c : r->upper;
}

uint32_t minim_unicode_downcase(uint32_t c) {
    const case_row *r = find_row(c);
    return r == NULL ? c : r->lower;
}

uint32_t minim_unicode_foldcase(uint32_t c) {
    const case_row *r = find_row(c);
    return r == NULL ? c : r->fold;
}

// Copies the 0-padded mapping `m` into `out`; returns its length.
static size_t copy_mapping(const uint32_t m[MINIM_UNICODE_MAX_MAPPING],
                           uint32_t out[MINIM_UNICODE_MAX_MAPPING]) {
    size_t len = 0;
    for (; len < MINIM_UNICODE_MAX_MAPPING && m[len] != 0; len++)
        out[len] = m[len];
    return len;
}

size_t minim_unicode_full_upcase(const uint32_t *s, size_t n, size_t i,
                                 uint32_t out[MINIM_UNICODE_MAX_MAPPING]) {
    (void)n;
    const full_row *r = find_full(s[i]);
    if (r != NULL)
        return copy_mapping(r->upper, out);
    out[0] = minim_unicode_upcase(s[i]);
    return 1;
}

// Whether `s[i]`, of the `n` code points `s`, ends a word: a cased letter is
// before it and none after, past case-ignorable code points (`Final_Sigma`).
static bool ends_word(const uint32_t *s, size_t n, size_t i) {
    size_t j = i;
    while (j > 0 && (minim_unicode_props(s[j - 1]) & MINIM_UNICODE_CASE_IGNORABLE))
        j--;
    if (j == 0 || !(minim_unicode_props(s[j - 1]) & MINIM_UNICODE_CASED))
        return false;
    size_t k = i + 1;
    while (k < n && (minim_unicode_props(s[k]) & MINIM_UNICODE_CASE_IGNORABLE))
        k++;
    return k == n || !(minim_unicode_props(s[k]) & MINIM_UNICODE_CASED);
}

size_t minim_unicode_full_downcase(const uint32_t *s, size_t n, size_t i,
                                   uint32_t out[MINIM_UNICODE_MAX_MAPPING]) {
    if (s[i] == 0x03A3 && ends_word(s, n, i)) {
        out[0] = 0x03C2;
        return 1;
    }
    const full_row *r = find_full(s[i]);
    if (r != NULL)
        return copy_mapping(r->lower, out);
    out[0] = minim_unicode_downcase(s[i]);
    return 1;
}

size_t minim_unicode_full_foldcase(const uint32_t *s, size_t n, size_t i,
                                   uint32_t out[MINIM_UNICODE_MAX_MAPPING]) {
    (void)n;
    const full_row *r = find_full(s[i]);
    if (r != NULL)
        return copy_mapping(r->fold, out);
    out[0] = minim_unicode_foldcase(s[i]);
    return 1;
}

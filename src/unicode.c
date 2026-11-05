/**
 * @file
 * Unicode properties and simple case mappings, from generated tables.
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

#include "unicode_data.inc"

static int compare_range(const void *key, const void *elt) {
    uint32_t c = *(const uint32_t *)key;
    const prop_range *r = elt;
    return c < r->first ? -1 : c > r->last;
}

static int compare_row(const void *key, const void *elt) {
    uint32_t c = *(const uint32_t *)key;
    const case_row *r = elt;
    return c < r->c ? -1 : c > r->c;
}

// The range of `props` holding `c`, or NULL.
static const prop_range *find_range(uint32_t c) {
    return bsearch(&c, props, sizeof(props) / sizeof(props[0]), sizeof(props[0]), compare_range);
}

// The row of `cases` for `c`, or NULL.
static const case_row *find_row(uint32_t c) {
    return bsearch(&c, cases, sizeof(cases) / sizeof(cases[0]), sizeof(cases[0]), compare_row);
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

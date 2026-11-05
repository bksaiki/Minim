/**
 * @file
 * Tests for Unicode properties and case mappings.
 */

#include <string.h>

#include "test.h"
#include "unicode.h"

enum {
    ALPHA = MINIM_UNICODE_ALPHABETIC,
    UPPER = MINIM_UNICODE_UPPER,
    LOWER = MINIM_UNICODE_LOWER,
    SPACE = MINIM_UNICODE_SPACE,
    DIGIT = MINIM_UNICODE_DIGIT,
    CASED = MINIM_UNICODE_CASED,
    IGNORABLE = MINIM_UNICODE_CASE_IGNORABLE,
};

static void test_props(void) {
    CHECK_EQ(minim_unicode_props('A'), ALPHA | UPPER | CASED);
    CHECK_EQ(minim_unicode_props('z'), ALPHA | LOWER | CASED);
    CHECK_EQ(minim_unicode_props('0'), DIGIT);
    CHECK_EQ(minim_unicode_props(' '), SPACE);
    CHECK_EQ(minim_unicode_props('.'), IGNORABLE);
    CHECK_EQ(minim_unicode_props(0x039B), ALPHA | UPPER | CASED);  // Λ
    CHECK_EQ(minim_unicode_props(0x03BB), ALPHA | LOWER | CASED);  // λ
    CHECK_EQ(minim_unicode_props(0x00DF), ALPHA | LOWER | CASED);  // ß
    CHECK_EQ(minim_unicode_props(0x0E01), ALPHA);                  // ก, uncased
    CHECK_EQ(minim_unicode_props(0x00A0), SPACE);                  // no-break space
    CHECK_EQ(minim_unicode_props(0x3000), SPACE);                  // ideographic space
    CHECK_EQ(minim_unicode_props(0x0378), 0);                      // unassigned
    CHECK_EQ(minim_unicode_props(0x10FFFF), 0);
}

static void test_digits(void) {
    CHECK_EQ(minim_unicode_digit('0'), 0);
    CHECK_EQ(minim_unicode_digit('9'), 9);
    CHECK_EQ(minim_unicode_digit(0x0E50), 0);   // ๐
    CHECK_EQ(minim_unicode_digit(0x0664), 4);   // ٤
    CHECK_EQ(minim_unicode_digit(0x0AE6), 0);   // ૦
    CHECK_EQ(minim_unicode_digit(0x1D7D9), 1);  // mathematical double-struck 1, in a run of 50
    CHECK_EQ(minim_unicode_digit('a'), -1);
    CHECK_EQ(minim_unicode_digit(0x2163), -1);  // Ⅳ, numeric but not a digit
}

static void test_cases(void) {
    CHECK_EQ(minim_unicode_upcase('a'), 'A');
    CHECK_EQ(minim_unicode_downcase('A'), 'a');
    CHECK_EQ(minim_unicode_foldcase('A'), 'a');
    CHECK_EQ(minim_unicode_upcase('1'), '1');
    CHECK_EQ(minim_unicode_upcase(0x03BB), 0x039B);     // λ
    CHECK_EQ(minim_unicode_downcase(0x039B), 0x03BB);   // Λ
    CHECK_EQ(minim_unicode_foldcase(0x039B), 0x03BB);
    CHECK_EQ(minim_unicode_upcase(0x00DF), 0x00DF);     // ß: SS only by full mapping
    CHECK_EQ(minim_unicode_foldcase(0x00DF), 0x00DF);
    CHECK_EQ(minim_unicode_foldcase(0x017F), 's');      // ſ
    CHECK_EQ(minim_unicode_upcase(0x017F), 'S');
    CHECK_EQ(minim_unicode_foldcase(0x1E9E), 0x00DF);   // ẞ, by simple folding
    CHECK_EQ(minim_unicode_downcase(0x10400), 0x10428); // Deseret, beyond the BMP
    CHECK_EQ(minim_unicode_upcase(0x10FFFF), 0x10FFFF);
}

// Whether `map` takes `s[i]`, of the `n` code points `s`, to the `len` code points `want`.
static bool maps(size_t (*map)(const uint32_t *, size_t, size_t, uint32_t *), const uint32_t *s,
                 size_t n, size_t i, const uint32_t *want, size_t len) {
    uint32_t out[MINIM_UNICODE_MAX_MAPPING];
    return map(s, n, i, out) == len && memcmp(out, want, len * sizeof(uint32_t)) == 0;
}

// Whether `map` takes the code point `c`, alone, to the `len` code points `want`.
static bool maps1(size_t (*map)(const uint32_t *, size_t, size_t, uint32_t *), uint32_t c,
                  const uint32_t *want, size_t len) {
    return maps(map, &c, 1, 0, want, len);
}

static void test_full_cases(void) {
    CHECK(maps1(minim_unicode_full_upcase, 'a', (uint32_t[]){'A'}, 1));
    CHECK(maps1(minim_unicode_full_upcase, 0x00DF, (uint32_t[]){'S', 'S'}, 2));        // ß
    CHECK(maps1(minim_unicode_full_downcase, 0x00DF, (uint32_t[]){0x00DF}, 1));
    CHECK(maps1(minim_unicode_full_foldcase, 0x00DF, (uint32_t[]){'s', 's'}, 2));
    CHECK(maps1(minim_unicode_full_downcase, 0x0130, (uint32_t[]){'i', 0x0307}, 2));  // İ
    CHECK(maps1(minim_unicode_full_upcase, 0x01F0, (uint32_t[]){'J', 0x030C}, 2));    // ǰ
    CHECK(maps1(minim_unicode_full_upcase, 0xFB03, (uint32_t[]){'F', 'F', 'I'}, 3));  // ﬃ
    CHECK(maps1(minim_unicode_full_foldcase, 0xFB03, (uint32_t[]){'f', 'f', 'i'}, 3));
    CHECK(maps1(minim_unicode_full_foldcase, 0x017F, (uint32_t[]){'s'}, 1));          // ſ
}

// A capital sigma lowercases to a final sigma after a cased letter and before
// none, past case-ignorable code points.
static void test_final_sigma(void) {
    const uint32_t *sigma = (uint32_t[]){0x03C3};
    const uint32_t *final = (uint32_t[]){0x03C2};
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){'A', 0x03A3}, 2, 1, final, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){'A', 0x03A3, ' '}, 3, 1, final, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){'A', '\'', 0x03A3}, 3, 2, final, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){0x03A3}, 1, 0, sigma, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){' ', 0x03A3}, 2, 1, sigma, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){'A', 0x03A3, 'B'}, 3, 1, sigma, 1));
    CHECK(maps(minim_unicode_full_downcase, (uint32_t[]){'A', 0x03A3, '\'', 'B'}, 4, 1, sigma, 1));
}

int main(void) {
    test_props();
    test_digits();
    test_cases();
    test_full_cases();
    test_final_sigma();
    return test_result();
}

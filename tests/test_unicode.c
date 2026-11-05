/**
 * @file
 * Tests for Unicode properties and simple case mappings.
 */

#include "test.h"
#include "unicode.h"

enum {
    ALPHA = MINIM_UNICODE_ALPHABETIC,
    UPPER = MINIM_UNICODE_UPPER,
    LOWER = MINIM_UNICODE_LOWER,
    SPACE = MINIM_UNICODE_SPACE,
    DIGIT = MINIM_UNICODE_DIGIT,
};

static void test_props(void) {
    CHECK_EQ(minim_unicode_props('A'), ALPHA | UPPER);
    CHECK_EQ(minim_unicode_props('z'), ALPHA | LOWER);
    CHECK_EQ(minim_unicode_props('0'), DIGIT);
    CHECK_EQ(minim_unicode_props(' '), SPACE);
    CHECK_EQ(minim_unicode_props('.'), 0);
    CHECK_EQ(minim_unicode_props(0x039B), ALPHA | UPPER);  // Λ
    CHECK_EQ(minim_unicode_props(0x03BB), ALPHA | LOWER);  // λ
    CHECK_EQ(minim_unicode_props(0x00DF), ALPHA | LOWER);  // ß
    CHECK_EQ(minim_unicode_props(0x0E01), ALPHA);          // ก, uncased
    CHECK_EQ(minim_unicode_props(0x00A0), SPACE);          // no-break space
    CHECK_EQ(minim_unicode_props(0x3000), SPACE);          // ideographic space
    CHECK_EQ(minim_unicode_props(0x0378), 0);              // unassigned
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

int main(void) {
    test_props();
    test_digits();
    test_cases();
    return test_result();
}

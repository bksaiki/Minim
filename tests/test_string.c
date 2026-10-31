/**
 * @file
 * Tests for strings.
 */

#include "gc.h"
#include "minim.h"
#include "test.h"

// Whether `s` holds exactly the `n` code points in `expect`.
static bool has_chars(minim_obj s, const uint32_t *expect, size_t n) {
    if (!minim_is_string(s) || minim_string_length(s) != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        if (minim_string_ref(s, i) != expect[i])
            return false;
    }
    return true;
}

#define CHECK_CHARS(s, ...)                                                \
    CHECK(has_chars((s), (const uint32_t[]){__VA_ARGS__},                  \
                    sizeof((const uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t)))

static void test_from_cstr(void) {
    CHECK_EQ(minim_string_length(minim_string_from_cstr("")), 0);
    CHECK_CHARS(minim_string_from_cstr("hello"), 'h', 'e', 'l', 'l', 'o');
    // 1-, 2-, 3-, and 4-byte sequences
    CHECK_CHARS(minim_string_from_cstr("aé€\U0001F600"), 'a', 0xE9, 0x20AC, 0x1F600);
}

static void test_invalid_utf8(void) {
    CHECK_CHARS(minim_string_from_cstr("\xff"), 0xFFFD);
    CHECK_CHARS(minim_string_from_cstr("a\xc3"), 'a', 0xFFFD);              // truncated
    CHECK_CHARS(minim_string_from_cstr("\xc0\xaf"), 0xFFFD, 0xFFFD);        // overlong
    CHECK_CHARS(minim_string_from_cstr("\xed\xa0\x80"), 0xFFFD, 0xFFFD, 0xFFFD);  // surrogate
    CHECK_CHARS(minim_string_from_cstr("\xf4\x90\x80\x80"), 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD);  // > U+10FFFF
}

static void test_ref_set(void) {
    minim_obj s = minim_make_string(3, 'x');
    CHECK_CHARS(s, 'x', 'x', 'x');
    minim_string_set(s, 1, 0x1F600);
    minim_string_set(s, 2, 0xE9);
    CHECK_CHARS(s, 'x', 0x1F600, 0xE9);
}

static void test_footprint(void) {
    // 8 B header + 4 B per char, rounded up to 16
    const size_t sizes[][2] = {{0, 16}, {2, 16}, {3, 32}, {6, 32}, {7, 48}};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t before = minim_gc.allocated;
        minim_make_string(sizes[i][0], 'a');
        CHECK_EQ(minim_gc.allocated - before, sizes[i][1]);
    }
}

static void test_gc(void) {
    minim_gc_collect_major();
    size_t base = minim_gc.live;  // the symbol table

    minim_obj s = minim_string_from_cstr("café");
    minim_obj v = MINIM_NIL;
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&s, &v, &lst);
    v = minim_make_vector(2, s);
    lst = minim_string_from_cstr("x");
    lst = minim_cons(lst, MINIM_NIL);

    minim_obj old = s;
    minim_gc_collect_major();
    CHECK(s != old);  // moved
    CHECK_CHARS(s, 'c', 'a', 'f', 0xE9);
    CHECK_EQ(minim_vector_ref(v, 0), s);
    CHECK_EQ(minim_vector_ref(v, 1), s);
    CHECK_CHARS(minim_car(lst), 'x');
    CHECK_EQ(minim_gc.live, base + 32 + 32 + 16 + 16);  // vector, "café", "x", pair
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_from_cstr();
    test_invalid_utf8();
    test_ref_set();
    test_footprint();
    test_gc();
    minim_shutdown();
    return test_result();
}

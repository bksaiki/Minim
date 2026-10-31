/**
 * @file
 * Tests for value encoding and predicates.
 */

#include <stdalign.h>

#include "minim.h"
#include "test.h"

static void test_fixnum(void) {
    const intptr_t values[] = {0, 1, -1, MINIM_FIXNUM_MIN, MINIM_FIXNUM_MAX};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        minim_obj x = minim_fixnum(values[i]);
        CHECK(minim_is_fixnum(x));
        CHECK_EQ(minim_fixnum_value(x), values[i]);
    }
}

static void test_fixnum_arith(void) {
    CHECK_EQ(minim_fixnum(2) + minim_fixnum(3), minim_fixnum(5));
    CHECK_EQ(minim_fixnum(2) - minim_fixnum(7), minim_fixnum(-5));
    CHECK_EQ(minim_fixnum_value(minim_fixnum(-4) + minim_fixnum(1)), -3);
}

static void test_char(void) {
    const uint32_t values[] = {0, 'a', 0x7F, 0xE9, 0xD7FF, 0xE000, 0x10FFFF};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        minim_obj x = minim_char(values[i]);
        CHECK(minim_is_char(x));
        CHECK_EQ(minim_char_value(x), values[i]);
    }
}

// Number of predicates `x` satisfies
static int count_predicates(minim_obj x) {
    return minim_is_fixnum(x) + minim_is_pair(x) + minim_is_symbol(x)
        + minim_is_typed(x) + minim_is_char(x) + minim_is_boolean(x)
        + minim_is_null(x);
}

static void test_immediates(void) {
    CHECK(minim_is_boolean(MINIM_FALSE));
    CHECK(minim_is_boolean(MINIM_TRUE));
    CHECK(minim_is_null(MINIM_NIL));
    CHECK(minim_is_char(minim_char('x')));

    const minim_obj typed[] = {MINIM_FALSE, MINIM_TRUE, MINIM_NIL, minim_char('x')};
    for (size_t i = 0; i < sizeof(typed) / sizeof(typed[0]); i++)
        CHECK_EQ(count_predicates(typed[i]), 1);

    // No predicates yet
    const minim_obj untyped[] = {MINIM_VOID, MINIM_EOF, MINIM_UNBOUND, MINIM_FORWARD};
    for (size_t i = 0; i < sizeof(untyped) / sizeof(untyped[0]); i++)
        CHECK_EQ(count_predicates(untyped[i]), 0);
}

static void test_no_heap_predicates(void) {
    const minim_obj values[] = {
        MINIM_FALSE, MINIM_TRUE, MINIM_NIL, MINIM_VOID, MINIM_EOF,
        MINIM_UNBOUND, minim_char('x'), minim_char(0x10FFFF),
        minim_fixnum(0), minim_fixnum(-1), minim_fixnum(MINIM_FIXNUM_MAX),
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        CHECK(!minim_is_pair(values[i]));
        CHECK(!minim_is_symbol(values[i]));
        CHECK(!minim_is_typed(values[i]));
    }
}

// Typed objects built on the stack, since there is no heap yet
static void test_typed(void) {
    alignas(16) minim_obj vec[2] = {minim_make_header(3, MINIM_TYPE_VECTOR)};
    alignas(16) minim_obj str[2] = {minim_make_header(3, MINIM_TYPE_STRING)};
    minim_obj v = (minim_obj)vec | MINIM_TAG_TYPED;
    minim_obj s = (minim_obj)str | MINIM_TAG_TYPED;

    CHECK(minim_is_vector(v));
    CHECK(!minim_is_string(v));
    CHECK(minim_is_string(s));
    CHECK(!minim_is_vector(s));
    CHECK_EQ(count_predicates(v), 1);
    CHECK_EQ(count_predicates(s), 1);
}

int main(void) {
    test_fixnum();
    test_fixnum_arith();
    test_char();
    test_immediates();
    test_no_heap_predicates();
    test_typed();
    return test_result();
}

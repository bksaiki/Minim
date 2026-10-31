/**
 * @file
 * Tests for the printer.
 */

#include "heap.h"
#include "minim.h"
#include "test.h"

static minim_obj list2(minim_obj a, minim_obj b) {
    MINIM_FRAME(&a, &b);
    minim_obj tail = minim_cons(b, MINIM_NIL);
    MINIM_RETURN(minim_cons(a, tail));
}

static minim_obj list3(minim_obj a, minim_obj b, minim_obj c) {
    MINIM_FRAME(&a);
    minim_obj tail = list2(b, c);
    MINIM_RETURN(minim_cons(a, tail));
}

static void test_immediates(void) {
    CHECK(prints_as(minim_fixnum(0), "0"));
    CHECK(prints_as(minim_fixnum(-42), "-42"));
    CHECK(prints_as(minim_fixnum(MINIM_FIXNUM_MAX), "1152921504606846975"));
    CHECK(prints_as(MINIM_TRUE, "#t"));
    CHECK(prints_as(MINIM_FALSE, "#f"));
    CHECK(prints_as(MINIM_NIL, "()"));
    CHECK(prints_as(MINIM_VOID, "#<void>"));
    CHECK(prints_as(MINIM_EOF, "#<eof>"));
    CHECK(prints_as(MINIM_UNBOUND, "#<unbound>"));
}

static void test_chars(void) {
    CHECK(prints_as(minim_char('a'), "#\\a"));
    CHECK(prints_as(minim_char(' '), "#\\space"));
    CHECK(prints_as(minim_char('\n'), "#\\newline"));
    CHECK(prints_as(minim_char('\t'), "#\\tab"));
    CHECK(prints_as(minim_char(0), "#\\nul"));
    CHECK(prints_as(minim_char(0x7F), "#\\x7f"));
    CHECK(prints_as(minim_char(0xE9), "#\\\u00e9"));
    CHECK(prints_as(minim_char(0x1F600), "#\\\U0001F600"));
}

static void test_strings(void) {
    CHECK(prints_as(minim_string_from_cstr(""), "\"\""));
    CHECK(prints_as(minim_string_from_cstr("a\"b\\c\nd\te"), "\"a\\\"b\\\\c\\nd\\te\""));
    CHECK(prints_as(minim_string_from_cstr("\x01"), "\"\\x1;\""));
    CHECK(prints_as(minim_string_from_cstr("caf\u00e9 \U0001F600"), "\"caf\u00e9 \U0001F600\""));
}

static void test_symbols(void) {
    CHECK(prints_as(minim_intern_cstr("foo"), "foo"));
    CHECK(prints_as(minim_intern_cstr("caf\u00e9"), "caf\u00e9"));
}

static void test_pairs(void) {
    minim_obj x = list3(minim_fixnum(1), minim_fixnum(2), minim_fixnum(3));
    MINIM_FRAME(&x);
    CHECK(prints_as(x, "(1 2 3)"));

    x = minim_cons(minim_fixnum(1), minim_fixnum(2));
    CHECK(prints_as(x, "(1 . 2)"));

    x = minim_cons(minim_fixnum(2), minim_fixnum(3));
    x = minim_cons(minim_fixnum(1), x);
    CHECK(prints_as(x, "(1 2 . 3)"));

    x = list2(minim_fixnum(1), MINIM_NIL);
    x = list2(x, minim_fixnum(2));
    CHECK(prints_as(x, "((1 ()) 2)"));
    MINIM_RETURN_VOID;
}

static void test_quote(void) {
    minim_obj quote = minim_intern_cstr("quote");
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&quote, &x);
    x = minim_intern_cstr("x");
    x = list2(quote, x);
    CHECK(prints_as(x, "'x"));

    x = list2(quote, x);
    CHECK(prints_as(x, "''x"));

    x = list3(quote, minim_fixnum(1), minim_fixnum(2));
    CHECK(prints_as(x, "(quote 1 2)"));

    x = minim_cons(quote, MINIM_NIL);
    CHECK(prints_as(x, "(quote)"));
    MINIM_RETURN_VOID;
}

static void test_vectors(void) {
    minim_obj v = minim_make_vector(0, MINIM_NIL);
    minim_obj inner = MINIM_NIL;
    MINIM_FRAME(&v, &inner);
    CHECK(prints_as(v, "#()"));

    v = minim_make_vector(3, MINIM_NIL);
    inner = minim_make_vector(1, minim_fixnum(2));
    minim_vector_set(v, 0, minim_fixnum(1));
    minim_vector_set(v, 1, inner);
    inner = minim_string_from_cstr("s");
    minim_vector_set(v, 2, inner);
    CHECK(prints_as(v, "#(1 #(2) \"s\")"));

    // Printing allocates nothing.
    size_t before = minim_heap.allocated;
    CHECK(prints_as(v, "#(1 #(2) \"s\")"));
    CHECK_EQ(minim_heap.allocated, before);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_immediates();
    test_chars();
    test_strings();
    test_symbols();
    test_pairs();
    test_quote();
    test_vectors();
    minim_shutdown();
    return test_result();
}

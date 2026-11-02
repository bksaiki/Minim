/**
 * @file
 * Tests for symbols.
 */

#include <stdio.h>

#include "minim.h"
#include "test.h"

static void test_same_name(void) {
    minim_obj a = minim_intern_cstr("foo");
    minim_obj str = MINIM_NIL;
    MINIM_FRAME(&a, &str);
    CHECK(minim_is_symbol(a));
    CHECK_EQ(minim_intern_cstr("foo"), a);

    str = minim_string_from_cstr("foo");
    CHECK_EQ(minim_intern(str), a);
    CHECK_EQ(minim_symbol_value(a), MINIM_UNBOUND);
    MINIM_RETURN_VOID;
}

static void test_distinct_names(void) {
    minim_obj a = minim_intern_cstr("café");
    minim_obj b = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    MINIM_FRAME(&a, &b, &c);
    b = minim_intern_cstr("cafe");
    c = minim_intern_cstr("");
    CHECK(a != b);
    CHECK(a != c);
    CHECK(b != c);
    CHECK_EQ(minim_string_ref(minim_symbol_name(a), 3), 0xE9);
    CHECK_EQ(minim_string_length(minim_symbol_name(c)), 0);
    MINIM_RETURN_VOID;
}

// The name is a copy, so mutating the original string doesn't rename the
// symbol.
static void test_name_copied(void) {
    minim_obj str = minim_string_from_cstr("bar");
    minim_obj sym = MINIM_NIL;
    MINIM_FRAME(&str, &sym);
    sym = minim_intern(str);
    CHECK(minim_symbol_name(sym) != str);

    minim_string_set(str, 0, 'c');
    CHECK_EQ(minim_string_ref(minim_symbol_name(sym), 0), 'b');
    CHECK(minim_intern(str) != sym);
    CHECK_EQ(minim_intern_cstr("bar"), sym);
    MINIM_RETURN_VOID;
}

static void test_gc(void) {
    minim_obj sym = minim_intern_cstr("kept");
    minim_obj val = MINIM_NIL;
    MINIM_FRAME(&sym, &val);
    val = minim_cons(minim_fixnum(1), MINIM_NIL);
    minim_set_symbol_value(sym, val);

    minim_obj old = sym;
    minim_gc_collect_major();
    CHECK(sym != old);  // moved
    CHECK_EQ(minim_intern_cstr("kept"), sym);
    CHECK_EQ(minim_symbol_value(sym), val);
    CHECK_EQ(minim_string_length(minim_symbol_name(sym)), 4);
    MINIM_RETURN_VOID;
}

// Symbols are never freed, even when unreferenced.
static void test_strong(void) {
    minim_set_symbol_value(minim_intern_cstr("ghost"), minim_fixnum(42));
    minim_gc_collect_major();
    CHECK_EQ(minim_symbol_value(minim_intern_cstr("ghost")), minim_fixnum(42));
}

static void test_growth(void) {
    const size_t n = 1000;
    char name[32];
    minim_obj syms = minim_make_vector(n, MINIM_NIL);
    MINIM_FRAME(&syms);
    for (size_t i = 0; i < n; i++) {
        snprintf(name, sizeof(name), "s%zu", i);
        minim_obj sym = minim_intern_cstr(name);
        minim_vector_set(syms, i, sym);
    }
    for (size_t i = 0; i < n; i++) {
        snprintf(name, sizeof(name), "s%zu", i);
        CHECK_EQ(minim_intern_cstr(name), minim_vector_ref(syms, i));
    }
    MINIM_RETURN_VOID;
}

static void test_uninterned(void) {
    minim_obj str = minim_string_from_cstr("u");
    minim_obj a = MINIM_NIL;
    minim_obj b = MINIM_NIL;
    MINIM_FRAME(&str, &a, &b);
    a = minim_make_uninterned(str);
    b = minim_make_uninterned(minim_symbol_name(a));
    CHECK(minim_is_symbol(a));
    CHECK(a != b);
    CHECK(minim_intern(str) != a);
    CHECK(minim_symbol_name(a) != str);                    // a mutable name is copied
    CHECK_EQ(minim_symbol_name(b), minim_symbol_name(a));  // an immutable one is shared
    CHECK_EQ(minim_symbol_value(a), MINIM_UNBOUND);

    // Survives a collection while referenced, value intact.
    minim_set_symbol_value(a, minim_fixnum(7));
    minim_gc_collect();
    CHECK_EQ(minim_symbol_value(a), minim_fixnum(7));
    CHECK_EQ(minim_string_ref(minim_symbol_name(a), 0), 'u');
    CHECK(minim_intern_cstr("u") != a);
    MINIM_RETURN_VOID;
}

static void test_props(void) {
    minim_obj sym = minim_intern_cstr("p");
    minim_obj k = MINIM_NIL;
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&sym, &k, &v);
    k = minim_intern_cstr("k");
    v = minim_make_vector(1, MINIM_NIL);
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), MINIM_FALSE);

    minim_putprop(sym, k, minim_fixnum(1));
    minim_putprop(sym, v, minim_fixnum(2));
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), minim_fixnum(1));
    CHECK_EQ(minim_getprop(sym, v, MINIM_FALSE), minim_fixnum(2));

    minim_putprop(sym, v, minim_fixnum(3));  // replaces
    CHECK_EQ(minim_getprop(sym, v, MINIM_FALSE), minim_fixnum(3));
    minim_remprop(sym, v);
    CHECK_EQ(minim_getprop(sym, v, MINIM_FALSE), MINIM_FALSE);
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), minim_fixnum(1));
    minim_remprop(sym, v);  // absent
    minim_remprop(sym, k);
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), MINIM_FALSE);

    // Keys compare by eq?: an equal vector isn't the key.
    minim_putprop(sym, v, minim_fixnum(4));
    v = minim_make_vector(1, MINIM_NIL);
    CHECK_EQ(minim_getprop(sym, v, MINIM_FALSE), MINIM_FALSE);
    MINIM_RETURN_VOID;
}

// Properties belong to one symbol, not to its name.
static void test_props_isolated(void) {
    minim_obj a = minim_intern_cstr("q");
    minim_obj u = MINIM_NIL;
    minim_obj b = MINIM_NIL;
    MINIM_FRAME(&a, &u, &b);
    u = minim_make_uninterned(minim_symbol_name(a));
    b = minim_intern_cstr("r");
    minim_putprop(a, a, minim_fixnum(1));
    CHECK_EQ(minim_getprop(u, a, MINIM_FALSE), MINIM_FALSE);
    CHECK_EQ(minim_getprop(b, a, MINIM_FALSE), MINIM_FALSE);
    minim_putprop(u, a, minim_fixnum(2));
    CHECK_EQ(minim_getprop(a, a, MINIM_FALSE), minim_fixnum(1));
    MINIM_RETURN_VOID;
}

static void test_props_gc(void) {
    minim_obj sym = minim_intern_cstr("pg");
    minim_obj k = MINIM_NIL;
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&sym, &k, &v);
    k = minim_make_vector(1, MINIM_NIL);
    v = minim_cons(minim_fixnum(1), MINIM_NIL);
    minim_putprop(sym, k, v);
    minim_gc_collect_major();
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), v);

    // An old symbol's young property (the write barrier).
    k = minim_intern_cstr("young");
    v = minim_cons(minim_fixnum(2), MINIM_NIL);
    minim_putprop(sym, k, v);
    minim_gc_collect();
    CHECK_EQ(minim_getprop(sym, k, MINIM_FALSE), v);
    CHECK_EQ(minim_car(v), minim_fixnum(2));
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_same_name();
    test_distinct_names();
    test_name_copied();
    test_gc();
    test_strong();
    test_growth();
    test_uninterned();
    test_props();
    test_props_isolated();
    test_props_gc();
    minim_shutdown();
    return test_result();
}

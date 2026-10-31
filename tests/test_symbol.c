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
    minim_gc_collect();
    CHECK(sym != old);  // moved
    CHECK_EQ(minim_intern_cstr("kept"), sym);
    CHECK_EQ(minim_symbol_value(sym), val);
    CHECK_EQ(minim_string_length(minim_symbol_name(sym)), 4);
    MINIM_RETURN_VOID;
}

// Symbols are never freed, even when unreferenced.
static void test_strong(void) {
    minim_set_symbol_value(minim_intern_cstr("ghost"), minim_fixnum(42));
    minim_gc_collect();
    CHECK_EQ(minim_symbol_value(minim_intern_cstr("ghost")), minim_fixnum(42));
}

static void test_growth(void) {
#ifdef MINIM_GC_STRESS
    const size_t n = 1000;  // every allocation collects
#else
    const size_t n = 10000;
#endif
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

int main(void) {
    minim_init();
    test_same_name();
    test_distinct_names();
    test_name_copied();
    test_gc();
    test_strong();
    test_growth();
    minim_shutdown();
    return test_result();
}

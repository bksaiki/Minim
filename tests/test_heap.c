/**
 * @file
 * Tests for allocation and object layouts.
 */

#include "heap.h"
#include "minim.h"
#include "test.h"

static bool aligned(minim_obj x) {
    return (x & ~(minim_obj)MINIM_TAG_MASK) % MINIM_ALIGN == 0;
}

static void test_list(void) {
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (intptr_t i = 100; i > 0; i--)
        lst = minim_cons(minim_fixnum(i), lst);

    intptr_t expect = 1;
    for (minim_obj p = lst; !minim_is_null(p); p = minim_cdr(p)) {
        CHECK(minim_is_pair(p));
        CHECK(aligned(p));
        CHECK_EQ(minim_fixnum_value(minim_car(p)), expect);
        expect++;
    }
    CHECK_EQ(expect, 101);

    minim_set_car(lst, MINIM_TRUE);
    minim_set_cdr(lst, MINIM_NIL);
    CHECK_EQ(minim_car(lst), MINIM_TRUE);
    CHECK_EQ(minim_cdr(lst), MINIM_NIL);
    MINIM_RETURN_VOID;
}

static void test_nested_vector(void) {
    minim_obj outer = minim_make_vector(3, MINIM_FALSE);
    MINIM_FRAME(&outer);
    CHECK(minim_is_vector(outer));
    CHECK(aligned(outer));
    CHECK_EQ(minim_vector_length(outer), 3);
    for (size_t i = 0; i < 3; i++) {
        CHECK_EQ(minim_vector_ref(outer, i), MINIM_FALSE);
        minim_obj inner = minim_make_vector(i, minim_fixnum((intptr_t)i));
        minim_vector_set(outer, i, inner);
    }
    for (size_t i = 0; i < 3; i++) {
        minim_obj inner = minim_vector_ref(outer, i);
        CHECK(minim_is_vector(inner));
        CHECK(aligned(inner));
        CHECK_EQ(minim_vector_length(inner), i);
        for (size_t j = 0; j < i; j++)
            CHECK_EQ(minim_vector_ref(inner, j), minim_fixnum((intptr_t)i));
    }
    MINIM_RETURN_VOID;
}

static void test_footprint(void) {
    size_t before = minim_heap.allocated;
    minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK_EQ(minim_heap.allocated - before, 16);

    // 8 B header + 8 B per element, rounded up to 16
    const size_t sizes[][2] = {{0, 16}, {1, 16}, {2, 32}, {3, 32}, {4, 48}};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        before = minim_heap.allocated;
        minim_make_vector(sizes[i][0], MINIM_NIL);
        CHECK_EQ(minim_heap.allocated - before, sizes[i][1]);
    }
}

static void test_large_vector(void) {
    const size_t n = 4 * MINIM_CHUNK_SIZE / sizeof(minim_obj);
    minim_obj v = minim_make_vector(n, MINIM_NIL);
    MINIM_FRAME(&v);
    minim_obj after = minim_cons(MINIM_FALSE, MINIM_NIL);

    CHECK(aligned(v));
    CHECK(aligned(after));
    CHECK_EQ(minim_vector_length(v), n);
    for (size_t i = 0; i < n; i++)
        minim_vector_set(v, i, minim_fixnum((intptr_t)i));
    for (size_t i = 0; i < n; i++)
        CHECK_EQ(minim_vector_ref(v, i), minim_fixnum((intptr_t)i));
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_list();
    test_nested_vector();
    test_footprint();
    test_large_vector();
    minim_shutdown();
    return test_result();
}

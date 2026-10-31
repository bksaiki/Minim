#include "heap.h"
#include "minim.h"
#include "test.h"

// Returns the list (0 1 ... n-1).
static minim_obj make_list(intptr_t n) {
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (intptr_t i = n - 1; i >= 0; i--)
        lst = minim_cons(minim_fixnum(i), lst);
    MINIM_RETURN(lst);
}

// Whether `lst` is (0 1 ... n-1).
static bool is_list(minim_obj lst, intptr_t n) {
    for (intptr_t i = 0; i < n; i++, lst = minim_cdr(lst)) {
        if (!minim_is_pair(lst) || minim_car(lst) != minim_fixnum(i))
            return false;
    }
    return minim_is_null(lst);
}

static void test_survive(void) {
    minim_obj lst = make_list(1000);
    minim_obj vec = MINIM_NIL;
    MINIM_FRAME(&lst, &vec);
    vec = minim_make_vector(10, MINIM_NIL);
    minim_vector_set(vec, 0, lst);
    for (size_t i = 1; i < 10; i++)
        minim_vector_set(vec, i, minim_fixnum((intptr_t)i));

    minim_obj old = lst;
    minim_gc_collect();
    CHECK(lst != old);  // moved
    CHECK(is_list(lst, 1000));
    CHECK_EQ(minim_vector_ref(vec, 0), lst);
    for (size_t i = 1; i < 10; i++)
        CHECK_EQ(minim_vector_ref(vec, i), minim_fixnum((intptr_t)i));
    MINIM_RETURN_VOID;
}

static void test_global(void) {
    static minim_obj global = MINIM_NIL;
    minim_gc_add_global(&global);
    global = make_list(10);
    minim_gc_collect();
    CHECK(is_list(global, 10));
    global = MINIM_NIL;
}

static void test_sharing(void) {
    minim_obj p = minim_cons(minim_fixnum(1), minim_fixnum(2));
    minim_obj q = p;
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&p, &q, &v);
    v = minim_make_vector(2, p);

    minim_gc_collect();
    CHECK_EQ(p, q);
    CHECK_EQ(minim_vector_ref(v, 0), p);
    CHECK_EQ(minim_vector_ref(v, 1), p);
    CHECK_EQ(minim_cdr(p), minim_fixnum(2));
    MINIM_RETURN_VOID;
}

static void test_cycles(void) {
    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&p, &v);
    minim_set_cdr(p, p);
    v = minim_make_vector(1, MINIM_NIL);
    minim_vector_set(v, 0, v);

    minim_gc_collect();
    CHECK_EQ(minim_car(p), minim_fixnum(1));
    CHECK_EQ(minim_cdr(p), p);
    CHECK_EQ(minim_vector_ref(v, 0), v);
    MINIM_RETURN_VOID;
}

// Keeps one list among many garbage lists, then collects.
static void collect_with_garbage(size_t base) {
    minim_obj keep = make_list(10);
    MINIM_FRAME(&keep);
    for (int i = 0; i < 1000; i++)
        make_list(10);

    minim_gc_collect();
    CHECK_EQ(minim_heap.live, base + 10 * 16);
    CHECK(is_list(keep, 10));
    MINIM_RETURN_VOID;
}

static void test_reclaim(void) {
    minim_gc_collect();
    size_t base = minim_heap.live;  // the symbol table
    collect_with_garbage(base);
    minim_gc_collect();
    CHECK_EQ(minim_heap.live, base);
}

static void test_bounded(void) {
#ifdef MINIM_GC_STRESS
    const int n = 100000;  // every allocation collects
#else
    const int n = 1000000;
#endif
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    size_t peak = 0;
    for (int i = 0; i < n; i++) {
        lst = (i % 100 == 0) ? MINIM_NIL : minim_cons(minim_fixnum(i), lst);
        if (minim_heap.size > peak)
            peak = minim_heap.size;
    }
    CHECK(peak <= 4 * 1024 * 1024);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_survive();
    test_global();
    test_sharing();
    test_cycles();
    test_reclaim();
    test_bounded();
    minim_shutdown();
    return test_result();
}

/**
 * @file
 * Tests for the collector.
 */

#include "gc.h"
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

static unsigned generation(minim_obj x) {
    return minim_chunk_of(x)->generation;
}

// The defaults with `max_generation` generations; returns the settings to restore.
static minim_gc_config use_generations(size_t max_generation) {
    minim_gc_config saved = use_default_gc_config();
    minim_gc_config config = minim_gc_get_config();
    config.max_generation = max_generation;
    minim_gc_set_config(config);
    return saved;
}

// The youngest generation the remembered object `x` may point to, or -1 if
// `x` isn't remembered.
static int remembered_youngest(minim_obj x) {
    minim_chunk *c = minim_chunk_of(x);
    return c->remembered[((x & ~(minim_obj)MINIM_TAG_MASK) - (uintptr_t)c) / MINIM_ALIGN] - 1;
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
    minim_gc_collect_major();
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
    minim_gc_collect_major();
    CHECK(is_list(global, 10));
    global = MINIM_NIL;
}

// Shared and cyclic structure stays shared.
static void test_sharing(void) {
    minim_obj p = minim_cons(minim_fixnum(1), minim_fixnum(2));
    minim_obj q = p;
    minim_obj c = MINIM_NIL;
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&p, &q, &c, &v);
    c = minim_cons(minim_fixnum(1), MINIM_NIL);
    minim_set_cdr(c, c);
    v = minim_make_vector(2, p);
    minim_vector_set(v, 1, v);

    minim_gc_collect_major();
    CHECK_EQ(p, q);
    CHECK_EQ(minim_cdr(p), minim_fixnum(2));
    CHECK_EQ(minim_car(c), minim_fixnum(1));
    CHECK_EQ(minim_cdr(c), c);
    CHECK_EQ(minim_vector_ref(v, 0), p);
    CHECK_EQ(minim_vector_ref(v, 1), v);
    MINIM_RETURN_VOID;
}

// A collection reclaims garbage only in the generations it collects.
static void test_reclaim(void) {
    minim_gc_collect_major();
    size_t base = minim_gc.live;  // the symbol table
    minim_obj lst = make_list(1000);
    MINIM_FRAME(&lst);
    for (int i = 0; i < 1000; i++)
        make_list(10);  // garbage
    minim_gc_collect_major();
    CHECK_EQ(minim_gc.live, base + 1000 * 16);
    CHECK(is_list(lst, 1000));

    lst = MINIM_NIL;
    minim_gc_collect_minor();
    CHECK_EQ(minim_gc.live, base + 1000 * 16);
    minim_gc_collect_major();
    CHECK_EQ(minim_gc.live, base);
    MINIM_RETURN_VOID;
}

static void test_bounded(void) {
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    size_t peak = 0;
    for (int i = 0; i < 1000000; i++) {
        lst = (i % 100 == 0) ? MINIM_NIL : minim_cons(minim_fixnum(i), lst);
        if (minim_gc.size > peak)
            peak = minim_gc.size;
    }
    CHECK(peak <= 4 * 1024 * 1024);
    MINIM_RETURN_VOID;
}

// A nursery collection doesn't move an old object a young one points to.
static void test_old_young_old(void) {
    minim_gc_config saved = use_default_gc_config();  // so allocating `b` doesn't collect
    minim_obj a = minim_cons(minim_fixnum(1), MINIM_NIL);
    minim_obj b = MINIM_NIL;
    minim_obj c = MINIM_NIL;
    MINIM_FRAME(&a, &b, &c);
    c = minim_cons(minim_fixnum(3), MINIM_NIL);
    minim_gc_collect_minor();
    b = minim_cons(c, MINIM_NIL);
    minim_set_cdr(a, b);
    minim_obj old_c = c;
    b = MINIM_NIL;
    c = MINIM_NIL;
    minim_gc_collect_minor();
    b = minim_cdr(a);
    CHECK_EQ(generation(b), 1);
    CHECK_EQ(minim_car(b), old_c);
    CHECK_EQ(minim_car(old_c), minim_fixnum(3));
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// The nursery's size sets how often the collector runs; setting it restarts
// the count.
static void test_config(void) {
    minim_gc_config saved = use_default_gc_config();
    for (int i = 0; i < 100; i++)
        minim_cons(MINIM_NIL, MINIM_NIL);  // within the default 1 MiB nursery
    CHECK_EQ(minim_gc.scheduled, 0);

    minim_gc_config config = minim_gc_get_config();
    config.nursery_bytes = 1;
    minim_gc_set_config(config);
    CHECK_EQ(minim_gc_get_config().nursery_bytes, 1);
    for (int i = 0; i < 8; i++)
        minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK_EQ(minim_gc.scheduled, 8);
    minim_gc_set_config(saved);
}

// An object ages a generation per collection that includes it, up to the
// oldest; a collection of g leaves older objects in place, and one of the
// oldest moves everything.
static void test_aging(void) {
    minim_gc_config saved = use_generations(3);
    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);
    MINIM_FRAME(&p);
    for (unsigned g = 0; g < 3; g++) {
        size_t before = minim_gc.collections[g];
        minim_gc_collect_generation(g);
        CHECK_EQ(generation(p), g + 1);
        CHECK_EQ(minim_gc.collections[g], before + 1);
    }
    minim_obj where = p;
    for (unsigned g = 0; g < 3; g++) {
        minim_gc_collect_generation(g);
        CHECK_EQ(p, where);
    }
    minim_gc_collect_generation(3);
    CHECK(p != where);
    CHECK_EQ(generation(p), 3);
    CHECK_EQ(minim_car(p), minim_fixnum(1));
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// The n-th scheduled collection collects the largest g with radix^g dividing n.
static void test_schedule(void) {
    minim_gc_config saved = use_generations(3);
    minim_gc_config config = minim_gc_get_config();
    const unsigned radix2[] = {0, 1, 0, 2, 0, 1, 0, 3};
    const unsigned radix1[] = {3, 3};
    const struct {
        size_t radix;
        const unsigned *expect;
        size_t n;
    } cases[] = {{2, radix2, 8}, {1, radix1, 2}};
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        config.radix = cases[c].radix;
        minim_gc_set_config(config);  // restarts the schedule
        for (size_t i = 0; i < cases[c].n; i++) {
            size_t before = minim_gc.collections[cases[c].expect[i]];
            minim_gc_collect();
            CHECK_EQ(minim_gc.collections[cases[c].expect[i]], before + 1);
        }
    }
    minim_gc_set_config(saved);
}

// Older objects set with each setter to point to new ones keep them alive as
// collections of every level age them.  Each setter writes a different object,
// so one barrier can't cover another.
static void test_older_to_younger(void) {
    minim_gc_config saved = use_generations(3);
    minim_obj pair3 = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj vec3 = MINIM_NIL;
    minim_obj sym3 = MINIM_NIL;
    minim_obj pair2 = MINIM_NIL;
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&pair3, &vec3, &sym3, &pair2, &lst);
    vec3 = minim_make_vector(1, MINIM_NIL);
    sym3 = minim_intern_cstr("older-to-younger");
    for (unsigned g = 0; g < 3; g++)
        minim_gc_collect_generation(g);  // into generation 3
    pair2 = minim_cons(MINIM_NIL, MINIM_NIL);
    for (unsigned g = 0; g < 2; g++)
        minim_gc_collect_generation(g);  // into generation 2
    CHECK_EQ(generation(pair3), 3);
    CHECK_EQ(generation(pair2), 2);

    lst = make_list(10);
    minim_set_car(pair3, lst);
    lst = make_list(11);
    minim_set_cdr(pair2, lst);
    lst = make_list(12);
    minim_vector_set(vec3, 0, lst);
    lst = make_list(13);
    minim_set_symbol_value(sym3, lst);
    lst = MINIM_NIL;
    for (unsigned g = 0; g <= 3; g++) {
        make_list(100);  // garbage
        minim_gc_collect_generation(g);
        CHECK(is_list(minim_car(pair3), 10));
        CHECK(is_list(minim_cdr(pair2), 11));
        CHECK(is_list(minim_vector_ref(vec3, 0), 12));
        CHECK(is_list(minim_symbol_value(sym3), 13));
    }
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// The barrier remembers an old object once, only for a heap value, with the
// youngest generation it may point to; it stays remembered while that
// generation is younger than its own.
static void test_remembered(void) {
    minim_gc_config saved = use_generations(3);
    minim_obj holder = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj young = MINIM_NIL;
    MINIM_FRAME(&holder, &young);
    for (unsigned g = 0; g < 3; g++)
        minim_gc_collect_generation(g);  // into generation 3
    minim_set_car(holder, minim_fixnum(1));
    minim_set_cdr(holder, MINIM_TRUE);
    CHECK_EQ(remembered_youngest(holder), -1);

    young = minim_cons(MINIM_NIL, MINIM_NIL);
    size_t len = minim_gc.remembered.len;
    for (int i = 0; i < 3; i++)
        minim_set_car(holder, young);
    CHECK_EQ(minim_gc.remembered.len, len + 1);
    CHECK_EQ(remembered_youngest(holder), 0);
    for (unsigned g = 0; g < 2; g++) {
        minim_gc_collect_generation(g);
        CHECK_EQ(generation(minim_car(holder)), g + 1);
        CHECK_EQ(remembered_youngest(holder), (int)g + 1);
    }

    // A write while remembered makes it a root for the nursery again.
    young = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_set_cdr(holder, young);
    CHECK_EQ(remembered_youngest(holder), 0);
    minim_gc_collect_minor();
    CHECK_EQ(generation(minim_cdr(holder)), 1);
    CHECK_EQ(remembered_youngest(holder), 1);
    minim_gc_collect_generation(1);
    CHECK_EQ(remembered_youngest(holder), 2);
    minim_gc_collect_generation(2);  // both referents reach generation 3
    CHECK_EQ(remembered_youngest(holder), -1);

    // Forgotten, so the next write remembers it again, until a major collection.
    young = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_set_car(holder, young);
    CHECK_EQ(remembered_youngest(holder), 0);
    minim_gc_collect_major();
    CHECK_EQ(remembered_youngest(holder), -1);
    CHECK_EQ(minim_car(holder), young);
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_survive();
    test_global();
    test_sharing();
    test_reclaim();
    test_bounded();
    test_old_young_old();
    test_config();
    test_aging();
    test_schedule();
    test_older_to_younger();
    test_remembered();
    minim_shutdown();
    return test_result();
}

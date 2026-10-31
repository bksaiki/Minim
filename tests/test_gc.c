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

static void test_sharing(void) {
    minim_obj p = minim_cons(minim_fixnum(1), minim_fixnum(2));
    minim_obj q = p;
    minim_obj v = MINIM_NIL;
    MINIM_FRAME(&p, &q, &v);
    v = minim_make_vector(2, p);

    minim_gc_collect_major();
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

    minim_gc_collect_major();
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

    minim_gc_collect_major();
    CHECK_EQ(minim_gc.live, base + 10 * 16);
    CHECK(is_list(keep, 10));
    MINIM_RETURN_VOID;
}

static void test_reclaim(void) {
    minim_gc_collect_major();
    size_t base = minim_gc.live;  // the symbol table
    collect_with_garbage(base);
    minim_gc_collect_major();
    CHECK_EQ(minim_gc.live, base);
}

static void test_bounded(void) {
    const int n = gc_stressed() ? 100000 : 1000000;
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    size_t peak = 0;
    for (int i = 0; i < n; i++) {
        lst = (i % 100 == 0) ? MINIM_NIL : minim_cons(minim_fixnum(i), lst);
        if (minim_gc.size > peak)
            peak = minim_gc.size;
    }
    CHECK(peak <= 4 * 1024 * 1024);
    MINIM_RETURN_VOID;
}

static bool is_old(minim_obj x) {
    return minim_chunk_of(x)->generation != 0;
}

// Survivors of a minor collection are promoted and stay put until a major one.
static void test_promotion(void) {
    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);
    MINIM_FRAME(&p);
    minim_gc_collect_minor();
    CHECK(is_old(p));
    minim_obj promoted = p;
    minim_gc_collect_minor();
    CHECK_EQ(p, promoted);
    minim_gc_collect_major();
    CHECK(p != promoted);
    CHECK(is_old(p));
    CHECK_EQ(minim_car(p), minim_fixnum(1));
    MINIM_RETURN_VOID;
}

// Collecting generation 0 is a minor collection; the oldest, a major one.
static void test_collect_generation(void) {
    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);
    MINIM_FRAME(&p);
    size_t minors = minim_gc.collections[0];
    size_t majors = minim_gc.collections[1];
    minim_gc_collect_generation(0);
    CHECK(is_old(p));
    minim_obj promoted = p;
    minim_gc_collect_generation(0);
    CHECK_EQ(p, promoted);
    minim_gc_collect_generation(1);
    CHECK(p != promoted);
    CHECK_EQ(minim_car(p), minim_fixnum(1));
    CHECK_EQ(minim_gc.collections[0] - minors, 2);
    CHECK_EQ(minim_gc.collections[1] - majors, 1);
    MINIM_RETURN_VOID;
}

// Old objects set to point to young ones, through each setter, keep them alive.
// Each setter writes a different object, so one barrier can't cover another.
static void test_old_to_young(void) {
    minim_obj pair = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj pair2 = MINIM_NIL;
    minim_obj vec = MINIM_NIL;
    minim_obj sym = MINIM_NIL;
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&pair, &pair2, &vec, &sym, &lst);
    pair2 = minim_cons(MINIM_NIL, MINIM_NIL);
    vec = minim_make_vector(2, MINIM_NIL);
    sym = minim_intern_cstr("old-to-young");
    minim_gc_collect_minor();

    lst = make_list(10);
    minim_set_car(pair, lst);
    lst = make_list(11);
    minim_set_cdr(pair2, lst);
    lst = make_list(12);
    minim_vector_set(vec, 1, lst);
    lst = make_list(13);
    minim_set_symbol_value(sym, lst);
    lst = MINIM_NIL;
    for (int i = 0; i < 10; i++) {
        make_list(100);  // garbage
        minim_gc_collect_minor();
    }
    CHECK(is_list(minim_car(pair), 10));
    CHECK(is_list(minim_cdr(pair2), 11));
    CHECK(is_list(minim_vector_ref(vec, 1), 12));
    CHECK(is_list(minim_symbol_value(sym), 13));
    MINIM_RETURN_VOID;
}

// An old object pointing to a young one that points back to an old one.
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
    CHECK(is_old(b));
    CHECK_EQ(minim_car(minim_car(b)), minim_fixnum(3));
    CHECK_EQ(minim_car(b), old_c);
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// Promoted garbage stays until a major collection.
static void test_major_reclaims(void) {
    minim_gc_collect_major();
    size_t base = minim_gc.live;
    minim_obj lst = make_list(1000);
    MINIM_FRAME(&lst);
    minim_gc_collect_minor();
    CHECK_EQ(minim_gc.live, base + 1000 * 16);
    lst = MINIM_NIL;
    minim_gc_collect_minor();
    CHECK_EQ(minim_gc.live, base + 1000 * 16);
    minim_gc_collect_major();
    CHECK_EQ(minim_gc.live, base);
    MINIM_RETURN_VOID;
}

// With a large structure live, short-lived allocation runs only minor
// collections.
static void test_generational(void) {
    minim_gc_config saved = use_default_gc_config();
    minim_obj keep = make_list(100000);  // 1.6 MB
    MINIM_FRAME(&keep);
    size_t minors = minim_gc.collections[0];
    size_t majors = minim_gc.collections[1];
    for (int i = 0; i < 1000000; i++)
        minim_cons(MINIM_NIL, MINIM_NIL);  // 16 MB
    CHECK(minim_gc.collections[0] - minors >= 15);
    CHECK_EQ(minim_gc.collections[1], majors);
    CHECK(is_list(keep, 100000));
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

static void test_config(void) {
    minim_gc_config saved = use_default_gc_config();
    size_t collections = minim_gc.collections[0] + minim_gc.collections[1];
    for (int i = 0; i < 100; i++)
        minim_cons(MINIM_NIL, MINIM_NIL);  // within the default 1 MiB nursery
    CHECK_EQ(minim_gc.collections[0] + minim_gc.collections[1], collections);

    // A 1-byte nursery collects at every allocation; radix 4 makes every 4th major.
    minim_gc_config config = minim_gc_get_config();
    config.nursery_bytes = 1;
    config.radix = 4;
    minim_gc_set_config(config);
    CHECK_EQ(minim_gc_get_config().radix, 4);
    size_t minors = minim_gc.collections[0];
    size_t majors = minim_gc.collections[1];
    for (int i = 0; i < 8; i++)
        minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK_EQ(minim_gc.collections[0] - minors, 6);
    CHECK_EQ(minim_gc.collections[1] - majors, 2);
    minim_gc_set_config(saved);
}

// Promotes one more pair onto `*keep` with each of `n` scheduled collections;
// returns how many were followed by a major collection.
static size_t majors_after(minim_obj *keep, int n) {
    size_t majors = minim_gc.collections[1];
    for (int i = 0; i < n; i++) {
        *keep = minim_cons(MINIM_NIL, *keep);
        minim_gc_collect();
    }
    return minim_gc.collections[1] - majors;
}

static void test_major_growth(void) {
    minim_gc_config saved = use_default_gc_config();  // no collections but the scheduled ones
    minim_obj keep = MINIM_NIL;
    MINIM_FRAME(&keep);
    minim_gc_config config = minim_gc_get_config();

    // Any growth past the size after the last major collection.
    config.major_growth = 1;
    config.major_min_bytes = 0;
    minim_gc_set_config(config);
    CHECK_EQ(majors_after(&keep, 4), 4);

    // Not before the old generation exceeds the minimum.
    config.major_min_bytes = 64 * 1024 * 1024;
    minim_gc_set_config(config);
    CHECK_EQ(majors_after(&keep, 4), 0);

    // Not before it grows by the factor.
    config.major_growth = 1000;
    config.major_min_bytes = 0;
    minim_gc_set_config(config);
    CHECK_EQ(majors_after(&keep, 4), 0);
    int n = 0;
    for (minim_obj x = keep; minim_is_pair(x); x = minim_cdr(x))
        n++;
    CHECK_EQ(n, 12);

    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// The barrier remembers an old object once, only for a heap value, until the
// next collection.
static void test_remembered(void) {
    minim_obj vec = minim_make_vector(2, MINIM_NIL);
    minim_obj young = MINIM_NIL;
    MINIM_FRAME(&vec, &young);
    minim_gc_collect_minor();
    CHECK_EQ(minim_gc.remembered.len, 0);

    minim_vector_set(vec, 0, minim_fixnum(1));
    minim_vector_set(vec, 1, MINIM_TRUE);
    CHECK_EQ(minim_gc.remembered.len, 0);

    young = minim_cons(MINIM_NIL, MINIM_NIL);
    for (int i = 0; i < 3; i++)
        minim_vector_set(vec, 0, young);
    CHECK_EQ(minim_gc.remembered.len, 1);
    minim_gc_collect_minor();
    CHECK_EQ(minim_gc.remembered.len, 0);

    // Remembered again after its bit was cleared, and forgotten by a major collection.
    young = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_vector_set(vec, 1, young);
    CHECK_EQ(minim_gc.remembered.len, 1);
    minim_gc_collect_major();
    CHECK_EQ(minim_gc.remembered.len, 0);
    CHECK_EQ(minim_vector_ref(vec, 1), young);
    MINIM_RETURN_VOID;
}

// A symbol interned into an old symbol table survives minor collections.
static void test_old_symbol_table(void) {
    minim_gc_collect_minor();
    minim_obj sym = minim_intern_cstr("interned-late");
    minim_obj str = MINIM_NIL;
    MINIM_FRAME(&sym, &str);
    for (int i = 0; i < 3; i++) {
        make_list(100);  // garbage
        minim_gc_collect_minor();
    }
    CHECK_EQ(minim_intern_cstr("interned-late"), sym);
    str = minim_symbol_name(sym);
    CHECK_EQ(minim_string_length(str), 13);
    MINIM_RETURN_VOID;
}

// The defaults with `max_generation` generations; returns the settings to restore.
static minim_gc_config use_generations(size_t max_generation) {
    minim_gc_config saved = use_default_gc_config();
    minim_gc_config config = minim_gc_get_config();
    config.max_generation = max_generation;
    minim_gc_set_config(config);
    return saved;
}

static unsigned generation(minim_obj x) {
    return minim_chunk_of(x)->generation;
}

// An object ages a generation per collection that includes it, up to the
// oldest; a collection of g leaves older objects in place.
static void test_aging(void) {
    minim_gc_config saved = use_generations(3);
    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);
    MINIM_FRAME(&p);
    for (unsigned g = 0; g < 3; g++) {
        minim_gc_collect_generation(g);
        CHECK_EQ(generation(p), g + 1);
    }
    minim_gc_collect_generation(3);
    CHECK_EQ(generation(p), 3);
    minim_obj where = p;
    for (unsigned g = 0; g < 3; g++) {
        minim_gc_collect_generation(g);
        CHECK_EQ(p, where);
    }
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
// collections of every level age them.
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

int main(void) {
    minim_init();
    test_survive();
    test_global();
    test_sharing();
    test_cycles();
    test_reclaim();
    test_bounded();
    test_promotion();
    test_collect_generation();
    test_old_to_young();
    test_old_young_old();
    test_major_reclaims();
    test_generational();
    test_config();
    test_major_growth();
    test_remembered();
    test_old_symbol_table();
    test_aging();
    test_schedule();
    test_older_to_younger();
    minim_shutdown();
    return test_result();
}

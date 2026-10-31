/**
 * @file
 * Tests for allocation and object layouts.
 */

#include "gc.h"
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
    size_t before = minim_gc.allocated;
    minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK_EQ(minim_gc.allocated - before, 16);

    // 8 B header + 8 B per element, rounded up to 16
    const size_t sizes[][2] = {{0, 16}, {1, 16}, {2, 32}, {3, 32}, {4, 48}};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        before = minim_gc.allocated;
        minim_make_vector(sizes[i][0], MINIM_NIL);
        CHECK_EQ(minim_gc.allocated - before, sizes[i][1]);
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

// Whether `x` lies in its chunk's data, and the chunk has generation `gen`.
static bool in_chunk(minim_obj x, unsigned gen) {
    minim_chunk *c = minim_chunk_of(x);
    uintptr_t p = x & ~(minim_obj)MINIM_TAG_MASK;
    return c->generation == gen && (uintptr_t)c->data <= p && p < (uintptr_t)c->data + c->size;
}

// Whether every chunk of every space is aligned and of the space's generation.
static bool chunks_aligned(void) {
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        const minim_space *s = &minim_gc.spaces[i];
        for (const minim_chunk *c = s->first; c != NULL; c = c->next) {
            if ((uintptr_t)c % MINIM_CHUNK_SIZE != 0 || c->generation != s->generation)
                return false;
        }
    }
    return true;
}

static void test_chunk_of(void) {
    minim_gc_config saved = use_default_gc_config();  // so the allocations below don't collect
    minim_obj pair = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_obj sym = MINIM_NIL;
    minim_obj str = MINIM_NIL;
    minim_obj big = MINIM_NIL;
    MINIM_FRAME(&pair, &sym, &str, &big);
    sym = minim_intern_cstr("chunk-of");
    str = minim_string_from_cstr("abc");
    big = minim_make_vector(2 * MINIM_CHUNK_SIZE / sizeof(minim_obj), MINIM_NIL);
    minim_obj after = minim_cons(MINIM_NIL, MINIM_NIL);  // not in the large chunk's tail
    CHECK(in_chunk(pair, 0));
    CHECK(in_chunk(str, 0));
    CHECK(in_chunk(big, 0));
    CHECK(in_chunk(after, 0));
    CHECK(minim_chunk_of(after) != minim_chunk_of(big));
    CHECK(chunks_aligned());

    // Survivors are copied into the per-kind spaces.
    minim_gc_collect_major();
    CHECK(in_chunk(pair, 1));
    CHECK(in_chunk(sym, 1));
    CHECK(in_chunk(str, 1));
    CHECK(in_chunk(big, 1));
    CHECK(chunks_aligned());
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

// Whether every spare chunk is standard-size, and there are at most the limit.
static bool spares_bounded(void) {
    size_t n = 0;
    for (const minim_chunk *c = minim_gc.spares.first; c != NULL; c = c->next, n++) {
        if (offsetof(minim_chunk, data) + c->size != MINIM_CHUNK_SIZE)
            return false;
    }
    return n == minim_gc.spares.len && n <= minim_heap_max_spares();
}

// A chunk freed by a collection is reused, once the quarantine has passed.
static void test_chunk_reuse(void) {
    minim_gc_config saved = use_default_gc_config();
    minim_gc_collect_major();
    minim_obj p = minim_cons(MINIM_NIL, MINIM_NIL);
    minim_chunk *chunk = minim_chunk_of(p);
    size_t ahead = minim_gc.spares.len;  // spares reused before `chunk`

    // Each minor collection makes the nursery's one chunk a spare, and the next
    // allocation takes the oldest spare past the quarantine.
    size_t reused_after = 0;
    for (size_t i = 1; i <= ahead + MINIM_CHUNK_QUARANTINE + 1 && reused_after == 0; i++) {
        minim_gc_collect_minor();
        p = minim_cons(MINIM_NIL, MINIM_NIL);
        if (minim_chunk_of(p) == chunk)
            reused_after = i;
    }
    CHECK(reused_after > MINIM_CHUNK_QUARANTINE);
    CHECK(spares_bounded());

    // Large chunks are freed, not kept.
    minim_make_vector(2 * MINIM_CHUNK_SIZE / sizeof(minim_obj), MINIM_NIL);
    minim_gc_collect_minor();
    CHECK(spares_bounded());

    // A major collection frees many chunks; only the limit is kept.
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (int i = 0; i < 100000; i++)
        lst = minim_cons(MINIM_NIL, lst);  // 1.6 MB, promoted
    lst = MINIM_NIL;
    minim_gc_collect_major();
    CHECK(spares_bounded());
    minim_gc_set_config(saved);
    MINIM_RETURN_VOID;
}

int main(void) {
    minim_init();
    test_list();
    test_nested_vector();
    test_footprint();
    test_large_vector();
    test_chunk_of();
    test_chunk_reuse();
    minim_shutdown();
    return test_result();
}

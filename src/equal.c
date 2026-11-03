/**
 * @file
 * Equivalence, hashing, and booleans.
 */

#include "prims.h"

#include <stdlib.h>

#include "heap.h"

static minim_obj prim_eq(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(args[0] == args[1]);
}

// Pairs of values `equal?` has yet to compare, as `a0, b0, a1, b1, ...`.  It
// starts in `small` and moves to the C heap if it outgrows it.
typedef struct pending {
    minim_obj *items;
    size_t len;
    size_t cap;
    minim_obj small[64];
} pending;

// Queues `a` and `b` for comparison unless they are the same object.
static void pending_push(pending *p, minim_obj a, minim_obj b) {
    if (a == b)
        return;
    if (p->len + 2 > p->cap) {
        p->cap *= 2;
        minim_obj *items = p->items == p->small
            ? malloc(p->cap * sizeof(minim_obj))
            : realloc(p->items, p->cap * sizeof(minim_obj));
        if (items == NULL)
            minim_out_of_memory();
        if (p->items == p->small)
            memcpy(items, p->small, p->len * sizeof(minim_obj));
        p->items = items;
    }
    p->items[p->len++] = a;
    p->items[p->len++] = b;
}

// Compares with an explicit stack, so deep structure can't overflow the C
// stack.  Nothing here allocates on the heap, so queued values stay valid,
// and nothing raises, so the stack is always freed.
static bool is_equal(minim_obj a, minim_obj b) {
    pending p = {.cap = sizeof(p.small) / sizeof(p.small[0])};
    p.items = p.small;
    pending_push(&p, a, b);

    bool result = true;
    while (result && p.len > 0) {
        b = p.items[--p.len];
        a = p.items[--p.len];
        if (minim_is_pair(a) && minim_is_pair(b)) {
            pending_push(&p, minim_cdr(a), minim_cdr(b));
            pending_push(&p, minim_car(a), minim_car(b));
        } else if (minim_is_vector(a) && minim_is_vector(b)) {
            result = minim_vector_length(a) == minim_vector_length(b);
            for (size_t i = 0; result && i < minim_vector_length(a); i++)
                pending_push(&p, minim_vector_ref(a, i), minim_vector_ref(b, i));
        } else if (minim_is_string(a) && minim_is_string(b)) {
            result = minim_string_equal(a, b);
        } else {
            result = false;  // distinct, and not both pairs, vectors, or strings
        }
    }

    if (p.items != p.small)
        free(p.items);
    return result;
}

static minim_obj prim_equal(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(is_equal(args[0], args[1]));
}

// Values `equal-hash` visits, at most; the rest of the structure is ignored,
// which keeps it O(1) and terminating on cyclic data.
enum { HASH_BUDGET = 64 };

// Kind markers, so different kinds with the same contents hash differently
enum { HASH_PAIR = 1, HASH_VECTOR, HASH_SYMBOL, HASH_CLOSURE };

// Hash agreeing with `equal?`: equal values visit the same nodes in the same
// order, so they consume the budget identically.
static uint64_t equal_hash(minim_obj x, int *budget) {
    if (--*budget < 0)
        return 0;
    if (minim_is_string(x))
        return minim_string_hash(x);
    if (minim_is_symbol(x))
        return minim_hash_mix(minim_string_hash(minim_symbol_name(x)), HASH_SYMBOL);
    if (minim_is_primitive(x))
        return minim_hash_mix(MINIM_HASH_INIT, (uintptr_t)minim_primitive_fn(x));
    if (minim_is_closure(x) || minim_is_continuation(x) || minim_is_error_object(x)
        || minim_is_record(x) || minim_is_port(x))
        return HASH_CLOSURE;  // equal? only when eq?
    if (minim_is_pair(x)) {
        uint64_t h = minim_hash_mix(MINIM_HASH_INIT, HASH_PAIR);
        h = minim_hash_mix(h, equal_hash(minim_car(x), budget));
        return minim_hash_mix(h, equal_hash(minim_cdr(x), budget));
    }
    if (minim_is_vector(x)) {
        uint64_t h = minim_hash_mix(MINIM_HASH_INIT, HASH_VECTOR);
        h = minim_hash_mix(h, minim_vector_length(x));
        for (size_t i = 0; i < minim_vector_length(x) && *budget > 0; i++)
            h = minim_hash_mix(h, equal_hash(minim_vector_ref(x, i), budget));
        return h;
    }
    return minim_hash_mix(MINIM_HASH_INIT, x);  // immediates
}

// Non-negative fixnum for the hash `h`
static minim_obj hash_result(uint64_t h) {
    return minim_fixnum((intptr_t)(h & (uint64_t)MINIM_FIXNUM_MAX));
}

static minim_obj prim_equal_hash(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    int budget = HASH_BUDGET;
    return hash_result(equal_hash(args[0], &budget));
}

static minim_obj prim_string_hash(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-hash: not a string", args[0]);
    return hash_result(minim_string_hash(args[0]));
}

static minim_obj prim_not(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(args[0] == MINIM_FALSE);
}

static minim_obj prim_is_boolean(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_boolean(args[0]));
}

const minim_prim_def minim_equal_prims[] = {
    {"eq?", prim_eq, 2, 2},
    {"eqv?", prim_eq, 2, 2},
    {"equal?", prim_equal, 2, 2},
    {"equal-hash", prim_equal_hash, 1, 1},
    {"string-hash", prim_string_hash, 1, 1},
    {"not", prim_not, 1, 1},
    {"boolean?", prim_is_boolean, 1, 1},
    {NULL, NULL, 0, 0},
};

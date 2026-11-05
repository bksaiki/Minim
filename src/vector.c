/**
 * @file
 * Vectors.
 */

#include "prims.h"

static minim_obj prim_vector(minim_obj *args, size_t n) {
    minim_obj v = minim_make_vector(n, MINIM_NIL);
    for (size_t i = 0; i < n; i++)
        minim_vector_set(v, i, args[i]);
    return v;
}

static minim_obj prim_make_vector(minim_obj *args, size_t n) {
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= 0,
          "make-vector: bad length", args[0]);
    minim_obj fill = n == 2 ? args[1] : minim_fixnum(0);
    return minim_make_vector((size_t)minim_fixnum_value(args[0]), fill);
}

static minim_obj prim_vector_ref(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_vector(args[0]), "vector-ref: not a vector", args[0]);
    size_t i = index_arg(args[1], minim_vector_length(args[0]), "vector-ref: bad index");
    return minim_vector_ref(args[0], i);
}

static minim_obj prim_vector_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_vector(args[0]), "vector-set!: not a vector", args[0]);
    size_t i = index_arg(args[1], minim_vector_length(args[0]), "vector-set!: bad index");
    minim_vector_set(args[0], i, args[2]);
    return MINIM_VOID;
}

static minim_obj prim_vector_length(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_vector(args[0]), "vector-length: not a vector", args[0]);
    return minim_fixnum((intptr_t)minim_vector_length(args[0]));
}

// The elements of the vector args[0] in the range args[1] and args[2], as a list.
static minim_obj prim_vector_to_list(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector->list: not a vector", args[0]);
    range r = range_args(args, n, 1, minim_vector_length(args[0]), "vector->list: bad index");
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = r.end; i > r.start; i--)
        lst = minim_cons(minim_vector_ref(args[0], i - 1), lst);
    MINIM_RETURN(lst);
}

// A new vector of the elements of the proper list args[0].
static minim_obj prim_list_to_vector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    size_t len = 0;
    minim_obj x = args[0];
    for (; minim_is_pair(x); x = minim_cdr(x))
        len++;
    check(minim_is_null(x), "list->vector: not a list", args[0]);

    minim_obj v = minim_make_vector(len, MINIM_NIL);
    x = args[0];  // the collection may have moved it
    for (size_t i = 0; i < len; i++, x = minim_cdr(x))
        minim_vector_set(v, i, minim_car(x));
    return v;
}

static minim_obj prim_is_vector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_vector(args[0]));
}

const minim_prim_def minim_vector_prims[] = {
    {"vector", prim_vector, 0, MINIM_VARIADIC},
    {"make-vector", prim_make_vector, 1, 2},
    {"vector-ref", prim_vector_ref, 2, 2},
    {"vector-set!", prim_vector_set, 3, 3},
    {"vector-length", prim_vector_length, 1, 1},
    {"vector->list", prim_vector_to_list, 1, 3},
    {"list->vector", prim_list_to_vector, 1, 1},
    {"vector?", prim_is_vector, 1, 1},
    {NULL, NULL, 0, 0},
};

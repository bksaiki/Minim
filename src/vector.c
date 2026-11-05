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

// A new vector of the elements of the vector args[0] in the range args[1] and args[2].
static minim_obj prim_vector_copy(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector-copy: not a vector", args[0]);
    range r = range_args(args, n, 1, minim_vector_length(args[0]), "vector-copy: bad index");
    minim_obj v = minim_make_vector(r.end - r.start, MINIM_NIL);
    for (size_t i = r.start; i < r.end; i++)
        minim_vector_set(v, i - r.start, minim_vector_ref(args[0], i));
    return v;
}

// Copies the elements of the vector args[2] in the range args[3] and args[4]
// into the vector args[0] at args[1]; backward when the target starts after
// the source, so a copy within one vector is correct.
static minim_obj prim_vector_copy_to(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector-copy!: not a vector", args[0]);
    size_t len = minim_vector_length(args[0]);
    size_t at = bound_arg(args[1], len, "vector-copy!: bad index");
    check(minim_is_vector(args[2]), "vector-copy!: not a vector", args[2]);
    range r = range_args(args, n, 3, minim_vector_length(args[2]), "vector-copy!: bad index");
    size_t count = r.end - r.start;
    check(count <= len - at, "vector-copy!: bad index", args[1]);
    if (at <= r.start) {
        for (size_t i = 0; i < count; i++)
            minim_vector_set(args[0], at + i, minim_vector_ref(args[2], r.start + i));
    } else {
        for (size_t i = count; i > 0; i--)
            minim_vector_set(args[0], at + i - 1, minim_vector_ref(args[2], r.start + i - 1));
    }
    return MINIM_VOID;
}

// Sets the elements of the vector args[0] in the range args[2] and args[3] to args[1].
static minim_obj prim_vector_fill(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector-fill!: not a vector", args[0]);
    range r = range_args(args, n, 2, minim_vector_length(args[0]), "vector-fill!: bad index");
    for (size_t i = r.start; i < r.end; i++)
        minim_vector_set(args[0], i, args[1]);
    return MINIM_VOID;
}

static minim_obj prim_vector_append(minim_obj *args, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        check(minim_is_vector(args[i]), "vector-append: not a vector", args[i]);
        total += minim_vector_length(args[i]);
    }
    minim_obj v = minim_make_vector(total, MINIM_NIL);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < minim_vector_length(args[i]); j++)
            minim_vector_set(v, k++, minim_vector_ref(args[i], j));
    }
    return v;
}

// A new vector of the chars of the string args[0] in the range args[1] and args[2].
static minim_obj prim_string_to_vector(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string->vector: not a string", args[0]);
    range r = range_args(args, n, 1, minim_string_length(args[0]), "string->vector: bad index");
    minim_obj v = minim_make_vector(r.end - r.start, MINIM_NIL);
    for (size_t i = r.start; i < r.end; i++)
        minim_vector_set(v, i - r.start, minim_char(minim_string_ref(args[0], i)));
    return v;
}

// A new string of the chars in the vector args[0] in the range args[1] and args[2].
static minim_obj prim_vector_to_string(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector->string: not a vector", args[0]);
    range r = range_args(args, n, 1, minim_vector_length(args[0]), "vector->string: bad index");
    for (size_t i = r.start; i < r.end; i++) {
        minim_obj c = minim_vector_ref(args[0], i);
        check(minim_is_char(c), "vector->string: not a char", c);
    }
    minim_obj s = minim_make_string(r.end - r.start, 0);
    for (size_t i = r.start; i < r.end; i++)
        minim_string_set(s, i - r.start, minim_char_value(minim_vector_ref(args[0], i)));
    return s;
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
    {"vector-copy", prim_vector_copy, 1, 3},
    {"vector-copy!", prim_vector_copy_to, 3, 5},
    {"vector-fill!", prim_vector_fill, 2, 4},
    {"vector-append", prim_vector_append, 0, MINIM_VARIADIC},
    {"string->vector", prim_string_to_vector, 1, 3},
    {"vector->string", prim_vector_to_string, 1, 3},
    {NULL, NULL, 0, 0},
};

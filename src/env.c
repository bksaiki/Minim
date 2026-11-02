/**
 * @file
 * Environments: local frames as vectors.
 */

#include "env.h"

#include <assert.h>

// A frame of n variables is `#(parent name ... value ...)`: its parent, then
// the n names, then their n values.
enum { FRAME_PARENT, FRAME_NAMES };

// Number of variables the formals `formals` declare.
static size_t formals_count(minim_obj formals) {
    size_t n = 0;
    for (; minim_is_pair(formals); formals = minim_cdr(formals))
        n++;
    return n + minim_is_symbol(formals);
}

// Number of variables in the frame `frame`.
static size_t frame_count(minim_obj frame) {
    return (minim_vector_length(frame) - 1) / 2;
}

// Slot of the value of `sym` in the frame `frame`, or 0 if it has none.
static size_t frame_slot(minim_obj frame, minim_obj sym) {
    size_t n = frame_count(frame);
    for (size_t i = 0; i < n; i++) {
        if (minim_vector_ref(frame, FRAME_NAMES + i) == sym)
            return FRAME_NAMES + n + i;
    }
    return 0;
}

// The innermost frame of `env` binding `sym`, with its slot in `*i`, or `'()`
// if `sym` is global.
static minim_obj find_frame(minim_obj env, minim_obj sym, size_t *i) {
    for (; !minim_is_null(env); env = minim_vector_ref(env, FRAME_PARENT)) {
        *i = frame_slot(env, sym);
        if (*i != 0)
            return env;
    }
    return MINIM_NIL;
}

// New frame in `parent` of `n` variables, every name and value `fill`.
static minim_obj make_frame(size_t n, minim_obj fill, minim_obj parent) {
    MINIM_FRAME(&parent);
    minim_obj frame = minim_make_vector(1 + 2 * n, fill);
    minim_vector_set(frame, FRAME_PARENT, parent);
    MINIM_RETURN(frame);
}

// Names the variables of `frame` from name `i` on after the formals
// `formals`; returns the index after them.
static size_t set_names(minim_obj frame, minim_obj formals, size_t i) {
    for (; minim_is_pair(formals); formals = minim_cdr(formals))
        minim_vector_set(frame, FRAME_NAMES + i++, minim_car(formals));
    if (minim_is_symbol(formals))
        minim_vector_set(frame, FRAME_NAMES + i++, formals);
    return i;
}

minim_obj minim_env_extend(minim_obj params, minim_obj *args, size_t n, minim_obj parent) {
    minim_obj rest = MINIM_NIL;
    MINIM_FRAME(&params, &parent, &rest);
    size_t m = 0;  // fixed parameters
    minim_obj tail = params;
    for (; minim_is_pair(tail); tail = minim_cdr(tail))
        m++;
    if (minim_is_symbol(tail)) {
        for (size_t j = n; j > m; j--)
            rest = minim_cons(args[j - 1], rest);
    }
    size_t count = m + minim_is_symbol(tail);
    minim_obj frame = make_frame(count, MINIM_NIL, parent);
    set_names(frame, params, 0);
    for (size_t j = 0; j < m; j++)
        minim_vector_set(frame, FRAME_NAMES + count + j, args[j]);
    if (minim_is_symbol(tail))
        minim_vector_set(frame, FRAME_NAMES + count + m, rest);
    MINIM_RETURN(frame);
}

minim_obj minim_env_extend_list(minim_obj params, minim_obj args, minim_obj parent) {
    MINIM_FRAME(&params, &args);
    size_t count = formals_count(params);
    minim_obj frame = make_frame(count, MINIM_NIL, parent);
    set_names(frame, params, 0);
    size_t i = FRAME_NAMES + count;
    for (; minim_is_pair(params); params = minim_cdr(params), args = minim_cdr(args))
        minim_vector_set(frame, i++, minim_car(args));
    if (minim_is_symbol(params))
        minim_vector_set(frame, i, args);
    MINIM_RETURN(frame);
}

minim_obj minim_env_frame(minim_obj clauses, minim_obj parent) {
    MINIM_FRAME(&clauses);
    size_t n = 0;
    for (minim_obj c = clauses; minim_is_pair(c); c = minim_cdr(c))
        n += formals_count(minim_car(minim_car(c)));
    minim_obj frame = make_frame(n, MINIM_UNBOUND, parent);
    size_t i = 0;
    for (; minim_is_pair(clauses); clauses = minim_cdr(clauses))
        i = set_names(frame, minim_car(minim_car(clauses)), i);
    MINIM_RETURN(frame);
}

minim_obj minim_env_lookup(minim_obj env, minim_obj sym) {
    size_t i;
    minim_obj frame = find_frame(env, sym, &i);
    minim_obj val = minim_is_null(frame) ? minim_symbol_value(sym) : minim_vector_ref(frame, i);
    if (val == MINIM_UNBOUND)
        minim_error("unbound variable", sym);
    return val;
}

void minim_env_define(minim_obj env, minim_obj sym, minim_obj val) {
    if (minim_is_null(env)) {
        minim_set_symbol_value(sym, val);
        return;
    }
    size_t i = frame_slot(env, sym);
    assert(i != 0);
    minim_vector_set(env, i, val);
}

void minim_env_set(minim_obj env, minim_obj sym, minim_obj val) {
    size_t i;
    minim_obj frame = find_frame(env, sym, &i);
    if (minim_is_null(frame)) {
        if (minim_symbol_value(sym) == MINIM_UNBOUND)
            minim_error("unbound variable", sym);
        minim_set_symbol_value(sym, val);
    } else {
        if (minim_vector_ref(frame, i) == MINIM_UNBOUND)
            minim_error("unbound variable", sym);
        minim_vector_set(frame, i, val);
    }
}

/**
 * @file
 * Primitive procedures: the ones the evaluator applies, the areas' tables, and
 * the argument checks they share.
 */

#ifndef MINIM_PRIMS_H
#define MINIM_PRIMS_H

#include "minim.h"

/** The `apply` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_apply(minim_obj *args, size_t n);

/** The `values` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_values(minim_obj *args, size_t n);

/** The `call-with-values` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_call_with_values(minim_obj *args, size_t n);

/** The `%call/cc` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_call_cc(minim_obj *args, size_t n);

/** The `load` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_load(minim_obj *args, size_t n);

/** The `%eval-expanded` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_eval_expanded(minim_obj *args, size_t n);

/** Binds the primitives as global symbol values; call after minim_symbol_init. */
void minim_prims_init(void);

/** A primitive to define: its name, function, and arities.  A table ends with a NULL name. */
typedef struct {
    const char *name;
    minim_prim_fn fn;
    intptr_t min_arity;
    intptr_t max_arity;
} minim_prim_def;

/** The primitives of each area, in its file. */
extern const minim_prim_def minim_number_prims[];
extern const minim_prim_def minim_char_prims[];
extern const minim_prim_def minim_pair_prims[];
extern const minim_prim_def minim_control_prims[];
extern const minim_prim_def minim_equal_prims[];
extern const minim_prim_def minim_vector_prims[];
extern const minim_prim_def minim_string_prims[];
extern const minim_prim_def minim_system_prims[];
extern const minim_prim_def minim_syntax_prims[];
extern const minim_prim_def minim_error_prims[];
extern const minim_prim_def minim_record_prims[];

/** Makes the tag that marks aliases; call before any alias is made. */
void minim_alias_init(void);

/** #t if `b`, else #f. */
static inline minim_obj boolean(bool b) {
    return b ? MINIM_TRUE : MINIM_FALSE;
}

/** Raises `msg` with irritant `x` unless `ok`. */
static inline void check(bool ok, const char *msg, minim_obj x) {
    if (!ok)
        minim_error(msg, x);
}

/**
 * Value of the index `x` into a sequence of length `len`; raises `msg`
 * otherwise.
 */
static inline size_t index_arg(minim_obj x, size_t len, const char *msg) {
    check(minim_is_fixnum(x) && minim_fixnum_value(x) >= 0
          && (size_t)minim_fixnum_value(x) < len, msg, x);
    return (size_t)minim_fixnum_value(x);
}

/** Value of the position `x` in `[0, max]`, for ranges; raises `msg` otherwise. */
static inline size_t bound_arg(minim_obj x, size_t max, const char *msg) {
    check(minim_is_fixnum(x) && minim_fixnum_value(x) >= 0
          && (size_t)minim_fixnum_value(x) <= max, msg, x);
    return (size_t)minim_fixnum_value(x);
}

/** Value of the fixnum `x`; raises `msg` otherwise. */
static inline intptr_t fixnum_arg(minim_obj x, const char *msg) {
    check(minim_is_fixnum(x), msg, x);
    return minim_fixnum_value(x);
}

/** Code of the char `x`; raises `msg` otherwise. */
static inline intptr_t char_arg(minim_obj x, const char *msg) {
    check(minim_is_char(x), msg, x);
    return minim_char_value(x);
}

/** Fixnum for `v`; raises `msg` if it is out of range. */
static inline minim_obj fixnum_result(intptr_t v, const char *msg) {
    if (v < MINIM_FIXNUM_MIN || v > MINIM_FIXNUM_MAX)
        minim_error(msg, MINIM_VOID);
    return minim_fixnum(v);
}

/**
 * Whether the keys of every adjacent pair of arguments satisfy `cmp`.  `key`
 * extracts a key from an argument, raising `msg` on the wrong type; every
 * argument is checked.
 */
static inline minim_obj compare(minim_obj *args, size_t n,
                                intptr_t (*key)(minim_obj, const char *),
                                bool (*cmp)(intptr_t, intptr_t), const char *msg) {
    bool result = true;
    intptr_t prev = 0;
    for (size_t i = 0; i < n; i++) {
        intptr_t v = key(args[i], msg);
        if (i > 0 && !cmp(prev, v))
            result = false;
        prev = v;
    }
    return boolean(result);
}

/** Comparisons of keys, for compare. */
static inline bool eq(intptr_t a, intptr_t b) { return a == b; }
static inline bool lt(intptr_t a, intptr_t b) { return a < b; }
static inline bool gt(intptr_t a, intptr_t b) { return a > b; }
static inline bool le(intptr_t a, intptr_t b) { return a <= b; }
static inline bool ge(intptr_t a, intptr_t b) { return a >= b; }

#endif

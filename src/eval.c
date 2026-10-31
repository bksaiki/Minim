/**
 * @file
 * Evaluator: an explicit-stack machine with continuation frames on the heap.
 */

#include "env.h"
#include "gc.h"
#include "minim.h"
#include "symbol.h"

// Continuation frames are vectors `#(kind next code env base)`.
enum { F_KIND, F_NEXT, F_CODE, F_ENV, F_BASE, FRAME_SIZE };

// Frame kinds, with what `code` holds.  Only K_ARGS uses `base`: the
// value-stack height where its call's values start.
enum {
    K_IF,      // branches `(then [else])`
    K_SEQ,     // remaining body
    K_DEFINE,  // symbol
    K_SET,     // symbol
    K_ARGS,    // remaining operands
};

static minim_obj make_frame(intptr_t kind, minim_obj next, minim_obj code, minim_obj env,
                            minim_obj base) {
    MINIM_FRAME(&next, &code, &env, &base);
    minim_obj f = minim_make_vector(FRAME_SIZE, MINIM_NIL);
    minim_vector_set(f, F_KIND, minim_fixnum(kind));
    minim_vector_set(f, F_NEXT, next);
    minim_vector_set(f, F_CODE, code);
    minim_vector_set(f, F_ENV, env);
    minim_vector_set(f, F_BASE, base);
    MINIM_RETURN(f);
}

// Length of the proper list `x`, or -1 if it isn't one.
static intptr_t list_length(minim_obj x) {
    intptr_t n = 0;
    for (; minim_is_pair(x); x = minim_cdr(x))
        n++;
    return minim_is_null(x) ? n : -1;
}

// Whether `params` is a valid lambda list: symbols, possibly with a rest symbol.
static bool valid_params(minim_obj params) {
    for (; minim_is_pair(params); params = minim_cdr(params)) {
        if (!minim_is_symbol(minim_car(params)))
            return false;
    }
    return minim_is_null(params) || minim_is_symbol(params);
}

// Whether a closure with `params` accepts `n` arguments.
static bool accepts(minim_obj params, size_t n) {
    size_t required = 0;
    for (; minim_is_pair(params); params = minim_cdr(params))
        required++;
    return minim_is_symbol(params) ? n >= required : n == required;
}

static bool primitive_accepts(minim_obj p, size_t n) {
    intptr_t max = minim_primitive_max_arity(p);
    return (intptr_t)n >= minim_primitive_min_arity(p)
        && (max == MINIM_VARIADIC || (intptr_t)n <= max);
}

minim_obj minim_eval(minim_obj expr) {
    minim_obj env = MINIM_NIL;
    minim_obj val = MINIM_VOID;
    minim_obj k = MINIM_NIL;  // continuation; '() returns from minim_eval
    minim_obj x = MINIM_NIL;  // scratch
    MINIM_FRAME(&expr, &env, &val, &k, &x);
    size_t base = 0;  // value-stack height where the applied call's values start

eval:  // Evaluates `expr` in `env`, then continues with `k`.
    if (minim_is_symbol(expr)) {
        val = minim_env_lookup(env, expr);
        goto ret;
    }
    if (!minim_is_pair(expr)) {
        if (minim_is_null(expr))
            minim_error("bad syntax", expr);
        val = expr;
        goto ret;
    }

    {
        intptr_t len = list_length(expr);
        if (len < 0)
            minim_error("bad syntax", expr);

        x = minim_car(expr);
        if (x == minim_sym_quote) {
            if (len != 2)
                minim_error("bad syntax", expr);
            val = minim_car(minim_cdr(expr));
            goto ret;
        } else if (x == minim_sym_if) {
            if (len != 3 && len != 4)
                minim_error("bad syntax", expr);
            k = make_frame(K_IF, k, minim_cdr(minim_cdr(expr)), env, MINIM_NIL);
            expr = minim_car(minim_cdr(expr));
            goto eval;
        } else if (x == minim_sym_define) {
            if (len != 3 || !minim_is_symbol(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            k = make_frame(K_DEFINE, k, minim_car(minim_cdr(expr)), env, MINIM_NIL);
            expr = minim_car(minim_cdr(minim_cdr(expr)));
            goto eval;
        } else if (x == minim_sym_set) {
            if (len != 3 || !minim_is_symbol(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            k = make_frame(K_SET, k, minim_car(minim_cdr(expr)), env, MINIM_NIL);
            expr = minim_car(minim_cdr(minim_cdr(expr)));
            goto eval;
        } else if (x == minim_sym_lambda) {
            if (len < 3 || !valid_params(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            val = minim_make_closure(minim_cdr(expr), env);
            goto ret;
        } else if (x == minim_sym_begin) {
            if (len == 1) {
                val = MINIM_VOID;
                goto ret;
            }
            x = minim_cdr(expr);
            goto sequence;
        }

        // Application: evaluate the operator, then each operand, onto the value stack.
        k = make_frame(K_ARGS, k, minim_cdr(expr), env, minim_fixnum((intptr_t)minim_values.len));
        expr = minim_car(expr);
        goto eval;
    }

sequence:  // Evaluates the non-empty body `x` in `env`; the last expression is a tail call.
    if (!minim_is_null(minim_cdr(x)))
        k = make_frame(K_SEQ, k, minim_cdr(x), env, MINIM_NIL);
    expr = minim_car(x);
    goto eval;

ret:  // Passes `val` to `k`.
    if (minim_is_null(k))
        MINIM_RETURN(val);
    x = k;
    k = minim_vector_ref(x, F_NEXT);
    switch (minim_fixnum_value(minim_vector_ref(x, F_KIND))) {
    case K_IF:
        env = minim_vector_ref(x, F_ENV);
        x = minim_vector_ref(x, F_CODE);
        if (val != MINIM_FALSE) {
            expr = minim_car(x);
            goto eval;
        }
        if (minim_is_null(minim_cdr(x))) {
            val = MINIM_VOID;
            goto ret;
        }
        expr = minim_car(minim_cdr(x));
        goto eval;
    case K_SEQ:
        env = minim_vector_ref(x, F_ENV);
        x = minim_vector_ref(x, F_CODE);
        goto sequence;
    case K_DEFINE:
        minim_env_define(minim_vector_ref(x, F_ENV), minim_vector_ref(x, F_CODE), val);
        val = MINIM_VOID;
        goto ret;
    case K_SET:
        minim_env_set(minim_vector_ref(x, F_ENV), minim_vector_ref(x, F_CODE), val);
        val = MINIM_VOID;
        goto ret;
    default:  // K_ARGS
        minim_values_push(val);
        env = minim_vector_ref(x, F_ENV);
        if (minim_is_pair(minim_vector_ref(x, F_CODE))) {
            minim_obj rest = minim_cdr(minim_vector_ref(x, F_CODE));
            k = make_frame(K_ARGS, k, rest, env, minim_vector_ref(x, F_BASE));
            expr = minim_car(minim_vector_ref(x, F_CODE));
            goto eval;
        }
        base = (size_t)minim_fixnum_value(minim_vector_ref(x, F_BASE));
        goto apply;
    }

apply:  // Applies the procedure at `base` to the values above it.
    {
        size_t n = minim_values.len - base - 1;
        x = minim_values.items[base];
        if (minim_is_primitive(x)) {
            if (!primitive_accepts(x, n))
                minim_error("arity mismatch", x);
            val = minim_primitive_fn(x)(minim_values.items + base + 1, n);
            minim_values.len = base;
            goto ret;
        }
        if (!minim_is_closure(x))
            minim_error("not a procedure", x);

        minim_obj params = minim_car(minim_closure_code(x));
        if (!accepts(params, n))
            minim_error("arity mismatch", x);
        env = minim_env_extend(params, minim_values.items + base + 1, n, minim_closure_env(x));
        minim_values.len = base;
        x = minim_cdr(minim_closure_code(x));
        goto sequence;
    }
}

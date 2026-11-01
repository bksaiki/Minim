/**
 * @file
 * Evaluator: an explicit-stack machine with continuation frames on the heap.
 */

#include <string.h>

#include "env.h"
#include "gc.h"
#include "minim.h"
#include "prims.h"
#include "read.h"
#include "symbol.h"

// Continuation frames are vectors `#(kind next code env base)`.
enum { F_KIND, F_NEXT, F_CODE, F_ENV, F_BASE, FRAME_SIZE };

// Frame kinds, with what `code` holds.  For K_ARGS, `base` is the value-stack
// height where its call's values start; for K_CALL_VALUES, the consumer's slot;
// for K_LET_VALUES, the new frame its clauses bind into.
enum {
    K_IF,             // branches `(then [else])`
    K_SEQ,            // remaining body
    K_DEFINE,         // symbol
    K_SET,            // symbol
    K_ARGS,           // remaining operands
    K_EXPAND,         // unused
    K_TOPLEVEL,       // remaining top-level forms
    K_CALL_VALUES,    // unused
    K_DEFINE_VALUES,  // formals
    K_LET_VALUES,     // `(clauses . body)`, the first clause's init being evaluated
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

// Whether `clauses` is a list of `(formals init)`.
static bool valid_clauses(minim_obj clauses) {
    for (; minim_is_pair(clauses); clauses = minim_cdr(clauses)) {
        minim_obj c = minim_car(clauses);
        if (list_length(c) != 2 || !valid_params(minim_car(c)))
            return false;
    }
    return minim_is_null(clauses);
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

// Pops the `n` values atop the value stack into a list.
static minim_obj pop_values(size_t n) {
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (; n > 0; n--) {
        lst = minim_cons(minim_gc.values.items[minim_gc.values.len - 1], lst);
        minim_gc.values.len--;
    }
    MINIM_RETURN(lst);
}

// Raises an error for the `n` values, other than one, atop the value stack.
static _Noreturn void wrong_number_of_values(size_t n) {
    minim_error("wrong number of values", pop_values(n));
}

// Binds `formals` in `env`, with minim_env_define, to the `n` values atop the
// value stack, and pops them.
static void bind_values(minim_obj env, minim_obj formals, size_t n) {
    if (!accepts(formals, n))
        wrong_number_of_values(n);
    MINIM_FRAME(&env, &formals);
    size_t start = minim_gc.values.len - n;
    size_t i = start;
    for (; minim_is_pair(formals); formals = minim_cdr(formals), i++)
        minim_env_define(env, minim_car(formals), minim_gc.values.items[i]);
    if (minim_is_symbol(formals))
        minim_env_define(env, formals, pop_values(minim_gc.values.len - i));
    minim_gc.values.len = start;
    MINIM_RETURN_VOID;
}

// Evaluates `expr`; returns its one value, or the list of its values if `many`.
static minim_obj eval(minim_obj expr, bool many) {
    minim_obj env = MINIM_NIL;
    minim_obj val = MINIM_VOID;
    minim_obj k = MINIM_NIL;  // continuation; '() returns from eval
    minim_obj x = MINIM_NIL;  // scratch
    minim_obj frame = MINIM_NIL;  // the frame a let-values form binds into
    size_t base = 0;  // value-stack height where the applied call's values start
    // Values being returned: 1 in `val`, or any other count atop the value stack.
    size_t nvals = 1;

    MINIM_FRAME(&expr, &env, &val, &k, &x, &frame);

toplevel:  // Expands `expr` with `%expand`, if bound, then evaluates it globally.
    env = MINIM_NIL;
    if (minim_symbol_value(minim_sym_expand) == MINIM_UNBOUND)
        goto eval;
    k = make_frame(K_EXPAND, k, MINIM_NIL, MINIM_NIL, MINIM_NIL);
    base = minim_gc.values.len;
    minim_values_push(minim_symbol_value(minim_sym_expand));
    minim_values_push(expr);
    goto apply;

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
        } else if (x == minim_sym_define || x == minim_sym_set) {
            if (len != 3 || !minim_is_symbol(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            intptr_t kind = x == minim_sym_define ? K_DEFINE : K_SET;
            k = make_frame(kind, k, minim_car(minim_cdr(expr)), env, MINIM_NIL);
            expr = minim_car(minim_cdr(minim_cdr(expr)));
            goto eval;
        } else if (x == minim_sym_define_values) {
            if (len != 3 || !valid_params(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            k = make_frame(K_DEFINE_VALUES, k, minim_car(minim_cdr(expr)), env, MINIM_NIL);
            expr = minim_car(minim_cdr(minim_cdr(expr)));
            goto eval;
        } else if (x == minim_sym_let_values || x == minim_sym_letrec_values) {
            if (len < 3 || !valid_clauses(minim_car(minim_cdr(expr))))
                minim_error("bad syntax", expr);
            frame = minim_env_extend(MINIM_NIL, NULL, 0, env);
            if (x == minim_sym_letrec_values)
                env = frame;
            x = minim_cdr(expr);
            goto let_values;
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
        x = minim_fixnum((intptr_t)minim_gc.values.len);
        k = make_frame(K_ARGS, k, minim_cdr(expr), env, x);
        expr = minim_car(expr);
        goto eval;
    }

sequence:  // Evaluates the non-empty body `x` in `env`; the last expression is a tail call.
    if (!minim_is_null(minim_cdr(x)))
        k = make_frame(K_SEQ, k, minim_cdr(x), env, MINIM_NIL);
    expr = minim_car(x);
    goto eval;

let_values:  // Evaluates the next init of `x`, `(clauses . body)`, in `env`, or else
             // the body in `frame`.
    if (minim_is_null(minim_car(x))) {
        env = frame;
        x = minim_cdr(x);
        goto sequence;
    }
    k = make_frame(K_LET_VALUES, k, x, env, frame);
    expr = minim_car(minim_cdr(minim_car(minim_car(x))));
    goto eval;

ret:  // Passes the values being returned to `k`.
    if (minim_is_null(k)) {
        if (many)
            val = nvals == 1 ? minim_cons(val, MINIM_NIL) : pop_values(nvals);
        else if (nvals != 1)
            wrong_number_of_values(nvals);
        MINIM_RETURN(val);
    }
    x = k;
    k = minim_vector_ref(x, F_NEXT);
    intptr_t kind = minim_fixnum_value(minim_vector_ref(x, F_KIND));
    if (nvals != 1) {
        if (kind == K_SEQ || kind == K_TOPLEVEL) {  // discarded
            minim_gc.values.len -= nvals;
            nvals = 1;
        } else if (kind != K_CALL_VALUES && kind != K_DEFINE_VALUES && kind != K_LET_VALUES) {
            wrong_number_of_values(nvals);
        }
    }
    switch (kind) {
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
    case K_EXPAND:
        expr = val;
        env = MINIM_NIL;
        goto eval;
    case K_TOPLEVEL:
        x = minim_vector_ref(x, F_CODE);
        if (minim_is_null(x)) {
            val = MINIM_VOID;
            goto ret;
        }
        k = make_frame(K_TOPLEVEL, k, minim_cdr(x), MINIM_NIL, MINIM_NIL);
        expr = minim_car(x);
        goto toplevel;
    case K_DEFINE_VALUES:
        if (nvals == 1)
            minim_values_push(val);
        bind_values(minim_vector_ref(x, F_ENV), minim_vector_ref(x, F_CODE), nvals);
        nvals = 1;
        val = MINIM_VOID;
        goto ret;
    case K_LET_VALUES:
        if (nvals == 1)
            minim_values_push(val);
        env = minim_vector_ref(x, F_ENV);
        frame = minim_vector_ref(x, F_BASE);
        x = minim_vector_ref(x, F_CODE);
        bind_values(frame, minim_car(minim_car(minim_car(x))), nvals);
        nvals = 1;
        x = minim_cons(minim_cdr(minim_car(x)), minim_cdr(x));
        goto let_values;
    case K_CALL_VALUES:  // apply the consumer, below the values
        if (nvals == 1)
            minim_values_push(val);
        nvals = 1;
        base = (size_t)minim_fixnum_value(minim_vector_ref(x, F_BASE));
        goto apply;
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
        size_t n = minim_gc.values.len - base - 1;
        x = minim_gc.values.items[base];
        if (minim_is_primitive(x)) {
            if (!primitive_accepts(x, n))
                minim_error("arity mismatch", x);
            if (minim_primitive_fn(x) == minim_prim_apply) {
                // [apply f a ... lst] becomes [f a ... elements of lst].
                x = minim_gc.values.items[--minim_gc.values.len];
                if (list_length(x) < 0)
                    minim_error("apply: not a list", x);
                memmove(minim_gc.values.items + base, minim_gc.values.items + base + 1,
                        (minim_gc.values.len - base - 1) * sizeof(minim_obj));
                minim_gc.values.len--;
                for (; minim_is_pair(x); x = minim_cdr(x))
                    minim_values_push(minim_car(x));
                goto apply;
            }
            if (minim_primitive_fn(x) == minim_prim_values) {
                // [values a ...] becomes the values a ... at base.
                minim_obj *items = minim_gc.values.items;
                if (n == 1) {
                    val = items[base + 1];
                    minim_gc.values.len = base;
                } else {
                    memmove(items + base, items + base + 1, n * sizeof(minim_obj));
                    minim_gc.values.len = base + n;
                    nvals = n;
                }
                goto ret;
            }
            if (minim_primitive_fn(x) == minim_prim_call_with_values) {
                // [call-with-values producer consumer] becomes [consumer producer], so
                // the producer's values return right above the consumer.
                minim_gc.values.items[base] = minim_gc.values.items[base + 2];
                minim_gc.values.len = base + 2;
                k = make_frame(K_CALL_VALUES, k, MINIM_NIL, MINIM_NIL,
                               minim_fixnum((intptr_t)base));
                base++;
                goto apply;
            }
            if (minim_primitive_fn(x) == minim_prim_load) {
                // Runs the file's forms as top-level forms, then returns void.
                if (!minim_is_string(minim_gc.values.items[base + 1]))
                    minim_error("load: not a string", minim_gc.values.items[base + 1]);
                x = minim_read_file(minim_gc.values.items[base + 1]);
                minim_gc.values.len = base;
                k = make_frame(K_TOPLEVEL, k, x, MINIM_NIL, MINIM_NIL);
                goto ret;
            }
            val = minim_primitive_fn(x)(minim_gc.values.items + base + 1, n);
            minim_gc.values.len = base;
            goto ret;
        }
        if (!minim_is_closure(x))
            minim_error("not a procedure", x);

        minim_obj params = minim_car(minim_closure_code(x));
        if (!accepts(params, n))
            minim_error("arity mismatch", x);
        env = minim_env_extend(params, minim_gc.values.items + base + 1, n, minim_closure_env(x));
        minim_gc.values.len = base;
        x = minim_cdr(minim_closure_code(x));
        goto sequence;
    }
}

minim_obj minim_eval(minim_obj expr) {
    return eval(expr, false);
}

minim_obj minim_eval_values(minim_obj expr) {
    return eval(expr, true);
}

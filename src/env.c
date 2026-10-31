/**
 * @file
 * Environments as association lists.
 */

#include "env.h"

// Binding `(sym . val)` of `sym` in the frame `bindings`, or `'()`.
static minim_obj frame_binding(minim_obj bindings, minim_obj sym) {
    for (; !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
        minim_obj b = minim_car(bindings);
        if (minim_car(b) == sym)
            return b;
    }
    return MINIM_NIL;
}

// Local binding of `sym` in `env`, or `'()` if it's global.
static minim_obj find_binding(minim_obj env, minim_obj sym) {
    for (; !minim_is_null(env); env = minim_cdr(env)) {
        minim_obj b = frame_binding(minim_car(env), sym);
        if (!minim_is_null(b))
            return b;
    }
    return MINIM_NIL;
}

minim_obj minim_env_extend(minim_obj params, minim_obj *args, size_t n, minim_obj parent) {
    minim_obj bindings = MINIM_NIL;
    minim_obj x = MINIM_NIL;
    MINIM_FRAME(&params, &parent, &bindings, &x);
    size_t i = 0;
    for (; minim_is_pair(params); params = minim_cdr(params), i++) {
        x = minim_cons(minim_car(params), args[i]);
        bindings = minim_cons(x, bindings);
    }
    if (minim_is_symbol(params)) {
        x = MINIM_NIL;
        for (size_t j = n; j > i; j--)
            x = minim_cons(args[j - 1], x);
        x = minim_cons(params, x);
        bindings = minim_cons(x, bindings);
    }
    MINIM_RETURN(minim_cons(bindings, parent));
}

minim_obj minim_env_lookup(minim_obj env, minim_obj sym) {
    minim_obj b = find_binding(env, sym);
    if (!minim_is_null(b))
        return minim_cdr(b);
    minim_obj val = minim_symbol_value(sym);
    if (val == MINIM_UNBOUND)
        minim_error("unbound variable", sym);
    return val;
}

void minim_env_define(minim_obj env, minim_obj sym, minim_obj val) {
    if (minim_is_null(env)) {
        minim_set_symbol_value(sym, val);
        return;
    }

    minim_obj b = frame_binding(minim_car(env), sym);
    if (!minim_is_null(b)) {
        minim_set_cdr(b, val);
        return;
    }
    MINIM_FRAME(&env);
    b = minim_cons(sym, val);
    b = minim_cons(b, minim_car(env));
    minim_set_car(env, b);
    MINIM_RETURN_VOID;
}

void minim_env_set(minim_obj env, minim_obj sym, minim_obj val) {
    minim_obj b = find_binding(env, sym);
    if (!minim_is_null(b))
        minim_set_cdr(b, val);
    else if (minim_symbol_value(sym) == MINIM_UNBOUND)
        minim_error("unbound variable", sym);
    else
        minim_set_symbol_value(sym, val);
}

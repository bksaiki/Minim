/**
 * @file
 * Primitive procedures.
 */

#include "prims.h"

#include "minim.h"

// Value of the fixnum `x`; raises `msg` otherwise.
static intptr_t fixnum_arg(minim_obj x, const char *msg) {
    if (!minim_is_fixnum(x))
        minim_error(msg, x);
    return minim_fixnum_value(x);
}

// Fixnum for `v`; raises `msg` if it is out of range.
static minim_obj fixnum_result(intptr_t v, const char *msg) {
    if (v < MINIM_FIXNUM_MIN || v > MINIM_FIXNUM_MAX)
        minim_error(msg, MINIM_VOID);
    return minim_fixnum(v);
}

// Fixnum sums never overflow intptr_t, so range checks after each step suffice.
static minim_obj prim_add(minim_obj *args, size_t n) {
    intptr_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += fixnum_arg(args[i], "+: not a fixnum");
        fixnum_result(sum, "+: fixnum overflow");
    }
    return minim_fixnum(sum);
}

static minim_obj prim_sub(minim_obj *args, size_t n) {
    intptr_t v = fixnum_arg(args[0], "-: not a fixnum");
    if (n == 1)
        return fixnum_result(-v, "-: fixnum overflow");
    for (size_t i = 1; i < n; i++) {
        v -= fixnum_arg(args[i], "-: not a fixnum");
        fixnum_result(v, "-: fixnum overflow");
    }
    return minim_fixnum(v);
}

static minim_obj prim_mul(minim_obj *args, size_t n) {
    intptr_t product = 1;
    for (size_t i = 0; i < n; i++) {
        intptr_t v = fixnum_arg(args[i], "*: not a fixnum");
        // Compare magnitudes, which fit in uint64_t, before multiplying.
        uint64_t a = product < 0 ? -(uint64_t)product : (uint64_t)product;
        uint64_t b = v < 0 ? -(uint64_t)v : (uint64_t)v;
        bool negative = (product < 0) != (v < 0);
        uint64_t limit = (uint64_t)MINIM_FIXNUM_MAX + (negative ? 1 : 0);
        if (b != 0 && a > limit / b)
            minim_error("*: fixnum overflow", MINIM_VOID);
        product *= v;
    }
    return minim_fixnum(product);
}

static minim_obj prim_num_eq(minim_obj *args, size_t n) {
    bool result = true;
    for (size_t i = 0; i < n; i++) {
        fixnum_arg(args[i], "=: not a fixnum");
        if (i > 0 && args[i] != args[i - 1])
            result = false;
    }
    return result ? MINIM_TRUE : MINIM_FALSE;
}

static minim_obj prim_lt(minim_obj *args, size_t n) {
    bool result = true;
    for (size_t i = 0; i < n; i++) {
        intptr_t v = fixnum_arg(args[i], "<: not a fixnum");
        if (i > 0 && minim_fixnum_value(args[i - 1]) >= v)
            result = false;
    }
    return result ? MINIM_TRUE : MINIM_FALSE;
}

static minim_obj prim_cons(minim_obj *args, size_t n) {
    (void)n;
    return minim_cons(args[0], args[1]);
}

static minim_obj prim_car(minim_obj *args, size_t n) {
    (void)n;
    if (!minim_is_pair(args[0]))
        minim_error("car: not a pair", args[0]);
    return minim_car(args[0]);
}

static minim_obj prim_cdr(minim_obj *args, size_t n) {
    (void)n;
    if (!minim_is_pair(args[0]))
        minim_error("cdr: not a pair", args[0]);
    return minim_cdr(args[0]);
}

static void define_primitive(const char *name, minim_prim_fn fn, intptr_t min, intptr_t max) {
    minim_obj sym = minim_intern_cstr(name);
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, fn, min, max);
    minim_set_symbol_value(sym, p);
    MINIM_RETURN_VOID;
}

void minim_prims_init(void) {
    define_primitive("+", prim_add, 0, MINIM_VARIADIC);
    define_primitive("-", prim_sub, 1, MINIM_VARIADIC);
    define_primitive("*", prim_mul, 0, MINIM_VARIADIC);
    define_primitive("=", prim_num_eq, 1, MINIM_VARIADIC);
    define_primitive("<", prim_lt, 1, MINIM_VARIADIC);
    define_primitive("cons", prim_cons, 2, 2);
    define_primitive("car", prim_car, 1, 1);
    define_primitive("cdr", prim_cdr, 1, 1);
}

/**
 * @file
 * Fixnum arithmetic, comparison, and conversion.
 */

#include "common.h"

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

static minim_obj prim_quotient(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t a = fixnum_arg(args[0], "quotient: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "quotient: not a fixnum");
    check(b != 0, "quotient: division by zero", args[1]);
    return fixnum_result(a / b, "quotient: fixnum overflow");
}

static minim_obj prim_remainder(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t a = fixnum_arg(args[0], "remainder: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "remainder: not a fixnum");
    check(b != 0, "remainder: division by zero", args[1]);
    return minim_fixnum(a % b);
}

// The digits of the fixnum args[0] in the radix args[1]: 2, 8, 10 (default), or 16.
static minim_obj prim_number_to_string(minim_obj *args, size_t n) {
    intptr_t v = fixnum_arg(args[0], "number->string: not a fixnum");
    intptr_t radix = 10;
    if (n == 2) {
        radix = minim_is_fixnum(args[1]) ? minim_fixnum_value(args[1]) : 0;
        check(radix == 2 || radix == 8 || radix == 10 || radix == 16,
              "number->string: bad radix", args[1]);
    }
    char buf[66];  // a sign, 64 binary digits, and a NUL
    char *p = buf + sizeof(buf);
    *--p = '\0';
    uintptr_t u = v < 0 ? -(uintptr_t)v : (uintptr_t)v;
    do {
        *--p = "0123456789abcdef"[u % (uintptr_t)radix];
        u /= (uintptr_t)radix;
    } while (u != 0);
    if (v < 0)
        *--p = '-';
    return minim_string_from_cstr(p);
}

static minim_obj prim_num_eq(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, eq, "=: not a fixnum");
}

static minim_obj prim_lt(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, lt, "<: not a fixnum");
}

static minim_obj prim_gt(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, gt, ">: not a fixnum");
}

static minim_obj prim_le(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, le, "<=: not a fixnum");
}

static minim_obj prim_ge(minim_obj *args, size_t n) {
    return compare(args, n, fixnum_arg, ge, ">=: not a fixnum");
}

static minim_obj prim_is_fixnum(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_fixnum(args[0]));
}

static minim_obj prim_is_zero(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(fixnum_arg(args[0], "zero?: not a fixnum") == 0);
}

const minim_prim_def minim_number_prims[] = {
    {"+", prim_add, 0, MINIM_VARIADIC},
    {"-", prim_sub, 1, MINIM_VARIADIC},
    {"*", prim_mul, 0, MINIM_VARIADIC},
    {"=", prim_num_eq, 1, MINIM_VARIADIC},
    {"<", prim_lt, 1, MINIM_VARIADIC},
    {">", prim_gt, 1, MINIM_VARIADIC},
    {"<=", prim_le, 1, MINIM_VARIADIC},
    {">=", prim_ge, 1, MINIM_VARIADIC},
    {"quotient", prim_quotient, 2, 2},
    {"remainder", prim_remainder, 2, 2},
    {"number->string", prim_number_to_string, 1, 2},
    {"fixnum?", prim_is_fixnum, 1, 1},
    {"zero?", prim_is_zero, 1, 1},
    {NULL, NULL, 0, 0},
};

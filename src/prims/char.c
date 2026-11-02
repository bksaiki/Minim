/**
 * @file
 * Characters.
 */

#include "common.h"

static minim_obj prim_is_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_char(args[0]));
}

static minim_obj prim_char_to_integer(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return minim_fixnum(char_arg(args[0], "char->integer: not a char"));
}

static minim_obj prim_integer_to_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t v = fixnum_arg(args[0], "integer->char: not a fixnum");
    check(v >= 0 && v <= 0x10FFFF && minim_is_scalar_value((uint32_t)v),
          "integer->char: not a scalar value", args[0]);
    return minim_char((uint32_t)v);
}

static minim_obj prim_char_eq(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, eq, "char=?: not a char");
}

static minim_obj prim_char_lt(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, lt, "char<?: not a char");
}

static minim_obj prim_char_gt(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, gt, "char>?: not a char");
}

static minim_obj prim_char_le(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, le, "char<=?: not a char");
}

static minim_obj prim_char_ge(minim_obj *args, size_t n) {
    return compare(args, n, char_arg, ge, "char>=?: not a char");
}

const minim_prim_def minim_char_prims[] = {
    {"char?", prim_is_char, 1, 1},
    {"char->integer", prim_char_to_integer, 1, 1},
    {"integer->char", prim_integer_to_char, 1, 1},
    {"char=?", prim_char_eq, 1, MINIM_VARIADIC},
    {"char<?", prim_char_lt, 1, MINIM_VARIADIC},
    {"char>?", prim_char_gt, 1, MINIM_VARIADIC},
    {"char<=?", prim_char_le, 1, MINIM_VARIADIC},
    {"char>=?", prim_char_ge, 1, MINIM_VARIADIC},
    {NULL, NULL, 0, 0},
};

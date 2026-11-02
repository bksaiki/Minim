/**
 * @file
 * Pairs and lists.
 */

#include "prims.h"

static minim_obj prim_cons(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return minim_cons(args[0], args[1]);
}

static minim_obj prim_car(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_pair(args[0]), "car: not a pair", args[0]);
    return minim_car(args[0]);
}

static minim_obj prim_cdr(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_pair(args[0]), "cdr: not a pair", args[0]);
    return minim_cdr(args[0]);
}

// `x` after the accessors `path` names (`"ad"` for cadr), applied right to left;
// raises `msg` with `x` if one meets a non-pair.
static minim_obj cxr(minim_obj x, const char *path, const char *msg) {
    minim_obj y = x;
    for (size_t i = strlen(path); i > 0; i--) {
        check(minim_is_pair(y), msg, x);
        y = path[i - 1] == 'a' ? minim_car(y) : minim_cdr(y);
    }
    return y;
}

#define CXR(path)                                                      \
    static minim_obj prim_c##path##r(minim_obj *args, size_t n) {      \
        MINIM_UNUSED(n);                                               \
        return cxr(args[0], #path, "c" #path "r: not a pair");         \
    }
CXR(aa)
CXR(ad)
CXR(da)
CXR(dd)
#undef CXR

static minim_obj prim_set_car(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_pair(args[0]), "set-car!: not a pair", args[0]);
    minim_set_car(args[0], args[1]);
    return MINIM_VOID;
}

static minim_obj prim_set_cdr(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_pair(args[0]), "set-cdr!: not a pair", args[0]);
    minim_set_cdr(args[0], args[1]);
    return MINIM_VOID;
}

static minim_obj prim_is_pair(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_pair(args[0]));
}

static minim_obj prim_is_null(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_null(args[0]));
}

static minim_obj prim_list(minim_obj *args, size_t n) {
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = n; i > 0; i--)
        lst = minim_cons(args[i - 1], lst);
    MINIM_RETURN(lst);
}

static minim_obj prim_length(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t len = 0;
    minim_obj x = args[0];
    for (; minim_is_pair(x); x = minim_cdr(x))
        len++;
    check(minim_is_null(x), "length: not a list", args[0]);
    return minim_fixnum(len);
}

const minim_prim_def minim_pair_prims[] = {
    {"cons", prim_cons, 2, 2},
    {"car", prim_car, 1, 1},
    {"cdr", prim_cdr, 1, 1},
    {"caar", prim_caar, 1, 1},
    {"cadr", prim_cadr, 1, 1},
    {"cdar", prim_cdar, 1, 1},
    {"cddr", prim_cddr, 1, 1},
    {"set-car!", prim_set_car, 2, 2},
    {"set-cdr!", prim_set_cdr, 2, 2},
    {"pair?", prim_is_pair, 1, 1},
    {"null?", prim_is_null, 1, 1},
    {"list", prim_list, 0, MINIM_VARIADIC},
    {"length", prim_length, 1, 1},
    {NULL, NULL, 0, 0},
};

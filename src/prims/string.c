/**
 * @file
 * Strings and symbols.
 */

#include "common.h"

static minim_obj prim_is_symbol(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_symbol(args[0]));
}

static minim_obj prim_string_length(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-length: not a string", args[0]);
    return minim_fixnum((intptr_t)minim_string_length(args[0]));
}

static minim_obj prim_string_ref(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-ref: not a string", args[0]);
    size_t i = index_arg(args[1], minim_string_length(args[0]), "string-ref: bad index");
    return minim_char(minim_string_ref(args[0], i));
}

static minim_obj prim_make_string(minim_obj *args, size_t n) {
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= 0,
          "make-string: bad length", args[0]);
    uint32_t c = n == 2 ? (uint32_t)char_arg(args[1], "make-string: not a char") : ' ';
    return minim_make_string((size_t)minim_fixnum_value(args[0]), c);
}

static minim_obj prim_string_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-set!: not a string", args[0]);
    check(!(minim_typed_header(args[0]) & MINIM_HEADER_IMMUTABLE),
          "string-set!: immutable string", args[0]);
    size_t i = index_arg(args[1], minim_string_length(args[0]), "string-set!: bad index");
    minim_string_set(args[0], i, (uint32_t)char_arg(args[2], "string-set!: not a char"));
    return MINIM_VOID;
}

static minim_obj prim_string_append(minim_obj *args, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        check(minim_is_string(args[i]), "string-append: not a string", args[i]);
        total += minim_string_length(args[i]);
    }

    // Allocation may move the arguments, so read their chars only afterward.
    minim_obj s = minim_make_string(total, 0);
    uint32_t *out = minim_string_chars(s);
    for (size_t i = 0; i < n; i++) {
        size_t len = minim_string_length(args[i]);
        memcpy(out, minim_string_chars(args[i]), len * sizeof(uint32_t));
        out += len;
    }
    return s;
}

static minim_obj prim_substring(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "substring: not a string", args[0]);
    size_t len = minim_string_length(args[0]);
    size_t start = bound_arg(args[1], len, "substring: bad index");
    size_t end = bound_arg(args[2], len, "substring: bad index");
    check(start <= end, "substring: bad index", args[1]);

    minim_obj s = minim_make_string(end - start, 0);
    memcpy(minim_string_chars(s), minim_string_chars(args[0]) + start,
           (end - start) * sizeof(uint32_t));
    return s;
}

// Compares `a` and `b` by code point: negative, zero, or positive.
static int string_compare(minim_obj a, minim_obj b) {
    size_t na = minim_string_length(a);
    size_t nb = minim_string_length(b);
    const uint32_t *ca = minim_string_chars(a);
    const uint32_t *cb = minim_string_chars(b);
    for (size_t i = 0; i < na && i < nb; i++) {
        if (ca[i] != cb[i])
            return ca[i] < cb[i] ? -1 : 1;
    }
    return na < nb ? -1 : na > nb;
}

// Whether every adjacent pair of strings satisfies `cmp`, applied to their
// three-way comparison and 0; raises `msg` on a non-string.
static minim_obj compare_strings(minim_obj *args, size_t n, bool (*cmp)(intptr_t, intptr_t),
                                 const char *msg) {
    bool result = true;
    for (size_t i = 0; i < n; i++) {
        check(minim_is_string(args[i]), msg, args[i]);
        if (i > 0 && !cmp(string_compare(args[i - 1], args[i]), 0))
            result = false;
    }
    return boolean(result);
}

static minim_obj prim_string_eq(minim_obj *args, size_t n) {
    return compare_strings(args, n, eq, "string=?: not a string");
}

static minim_obj prim_string_lt(minim_obj *args, size_t n) {
    return compare_strings(args, n, lt, "string<?: not a string");
}

static minim_obj prim_string_gt(minim_obj *args, size_t n) {
    return compare_strings(args, n, gt, "string>?: not a string");
}

static minim_obj prim_string_le(minim_obj *args, size_t n) {
    return compare_strings(args, n, le, "string<=?: not a string");
}

static minim_obj prim_string_ge(minim_obj *args, size_t n) {
    return compare_strings(args, n, ge, "string>=?: not a string");
}

static minim_obj prim_string_to_list(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string->list: not a string", args[0]);
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = minim_string_length(args[0]); i > 0; i--)
        lst = minim_cons(minim_char(minim_string_ref(args[0], i - 1)), lst);
    MINIM_RETURN(lst);
}

static minim_obj prim_list_to_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    size_t len = 0;
    minim_obj x = args[0];
    for (; minim_is_pair(x); x = minim_cdr(x), len++)
        check(minim_is_char(minim_car(x)), "list->string: not a char", minim_car(x));
    check(minim_is_null(x), "list->string: not a list", args[0]);

    minim_obj s = minim_make_string(len, 0);
    uint32_t *out = minim_string_chars(s);
    for (x = args[0]; minim_is_pair(x); x = minim_cdr(x))
        *out++ = minim_char_value(minim_car(x));
    return s;
}

static minim_obj prim_is_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_string(args[0]));
}

// Returns the symbol's immutable name itself.
static minim_obj prim_symbol_to_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "symbol->string: not a symbol", args[0]);
    return minim_symbol_name(args[0]);
}

static minim_obj prim_string_to_symbol(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string->symbol: not a string", args[0]);
    return minim_intern(args[0]);
}

const minim_prim_def minim_string_prims[] = {
    {"symbol?", prim_is_symbol, 1, 1},
    {"string-length", prim_string_length, 1, 1},
    {"string-ref", prim_string_ref, 2, 2},
    {"string?", prim_is_string, 1, 1},
    {"make-string", prim_make_string, 1, 2},
    {"string-set!", prim_string_set, 3, 3},
    {"string-append", prim_string_append, 0, MINIM_VARIADIC},
    {"substring", prim_substring, 3, 3},
    {"string=?", prim_string_eq, 1, MINIM_VARIADIC},
    {"string<?", prim_string_lt, 1, MINIM_VARIADIC},
    {"string>?", prim_string_gt, 1, MINIM_VARIADIC},
    {"string<=?", prim_string_le, 1, MINIM_VARIADIC},
    {"string>=?", prim_string_ge, 1, MINIM_VARIADIC},
    {"string->list", prim_string_to_list, 1, 1},
    {"list->string", prim_list_to_string, 1, 1},
    {"symbol->string", prim_symbol_to_string, 1, 1},
    {"string->symbol", prim_string_to_symbol, 1, 1},
    {NULL, NULL, 0, 0},
};

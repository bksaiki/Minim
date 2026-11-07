/**
 * @file
 * Strings and symbols.
 */

#include "prims.h"
#include "unicode.h"

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

// Raises `msg` if the string `s` is immutable.
static void check_mutable(minim_obj s, const char *msg) {
    check(!(minim_typed_header(s) & MINIM_HEADER_IMMUTABLE), msg, s);
}

static minim_obj prim_is_mutable_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_string(args[0])
                   && !(minim_typed_header(args[0]) & MINIM_HEADER_IMMUTABLE));
}

static minim_obj prim_string_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-set!: not a string", args[0]);
    check_mutable(args[0], "string-set!: immutable string");
    size_t i = index_arg(args[1], minim_string_length(args[0]), "string-set!: bad index");
    minim_string_set(args[0], i, (uint32_t)char_arg(args[2], "string-set!: not a char"));
    return MINIM_VOID;
}

// A full case mapping of `s[i]`, of the `n` code points `s`, into `out`;
// returns its length.
typedef size_t (*case_mapping)(const uint32_t *s, size_t n, size_t i,
                               uint32_t out[MINIM_UNICODE_MAX_MAPPING]);

// A new string of `map` applied to each char of the string args[0]; raises `msg`
// on a non-string.
static minim_obj map_case(minim_obj *args, case_mapping map, const char *msg) {
    check(minim_is_string(args[0]), msg, args[0]);
    size_t n = minim_string_length(args[0]);
    const uint32_t *in = minim_string_chars(args[0]);
    uint32_t buf[MINIM_UNICODE_MAX_MAPPING];
    size_t len = 0;
    for (size_t i = 0; i < n; i++)
        len += map(in, n, i, buf);

    minim_obj s = minim_make_string(len, 0);
    in = minim_string_chars(args[0]);  // the allocation may have moved it
    uint32_t *out = minim_string_chars(s);
    for (size_t i = 0; i < n; i++)
        out += map(in, n, i, out);
    return s;
}

static minim_obj prim_string_upcase(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return map_case(args, minim_unicode_full_upcase, "string-upcase: not a string");
}

static minim_obj prim_string_downcase(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return map_case(args, minim_unicode_full_downcase, "string-downcase: not a string");
}

static minim_obj prim_string_foldcase(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return map_case(args, minim_unicode_full_foldcase, "string-foldcase: not a string");
}

// The chars of the string args[0] in the range args[1] and args[2], as a list.
static minim_obj prim_string_to_list(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string->list: not a string", args[0]);
    range r = range_args(args, n, 1, minim_string_length(args[0]), "string->list: bad index");
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = r.end; i > r.start; i--)
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

static minim_obj prim_getprop(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "%getprop: not a symbol", args[0]);
    return minim_getprop(args[0], args[1], args[2]);
}

static minim_obj prim_putprop(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "%putprop!: not a symbol", args[0]);
    minim_putprop(args[0], args[1], args[2]);
    return MINIM_VOID;
}

const minim_prim_def minim_string_prims[] = {
    {"symbol?", prim_is_symbol, 1, 1},
    {"string-length", prim_string_length, 1, 1},
    {"string-ref", prim_string_ref, 2, 2},
    {"string?", prim_is_string, 1, 1},
    {"make-string", prim_make_string, 1, 2},
    {"string-set!", prim_string_set, 3, 3},
    {"mutable-string?", prim_is_mutable_string, 1, 1},
    {"string-upcase", prim_string_upcase, 1, 1},
    {"string-downcase", prim_string_downcase, 1, 1},
    {"string-foldcase", prim_string_foldcase, 1, 1},
    {"string->list", prim_string_to_list, 1, 3},
    {"list->string", prim_list_to_string, 1, 1},
    {"symbol->string", prim_symbol_to_string, 1, 1},
    {"string->symbol", prim_string_to_symbol, 1, 1},
    {"%getprop", prim_getprop, 3, 3},
    {"%putprop!", prim_putprop, 3, 3},
    {NULL, NULL, 0, 0},
};

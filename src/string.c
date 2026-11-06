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

// The symbol `x`, as a key for compare; raises `msg` otherwise.
static intptr_t symbol_key(minim_obj x, const char *msg) {
    check(minim_is_symbol(x), msg, x);
    return (intptr_t)x;
}

static minim_obj prim_symbol_eq(minim_obj *args, size_t n) {
    return compare(args, n, symbol_key, eq, "symbol=?: not a symbol");
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

static minim_obj prim_string(minim_obj *args, size_t n) {
    for (size_t i = 0; i < n; i++)
        check(minim_is_char(args[i]), "string: not a char", args[i]);
    minim_obj s = minim_make_string(n, 0);
    for (size_t i = 0; i < n; i++)
        minim_string_set(s, i, minim_char_value(args[i]));
    return s;
}

static minim_obj prim_string_set(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "string-set!: not a string", args[0]);
    check_mutable(args[0], "string-set!: immutable string");
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

// A new string of the chars of the string args[0] in the range args[1] and
// args[2]; raises `type_msg` or `index_msg`.
static minim_obj copy_range(minim_obj *args, size_t n, const char *type_msg,
                            const char *index_msg) {
    check(minim_is_string(args[0]), type_msg, args[0]);
    range r = range_args(args, n, 1, minim_string_length(args[0]), index_msg);
    minim_obj s = minim_make_string(r.end - r.start, 0);
    memcpy(minim_string_chars(s), minim_string_chars(args[0]) + r.start,
           (r.end - r.start) * sizeof(uint32_t));
    return s;
}

static minim_obj prim_substring(minim_obj *args, size_t n) {
    return copy_range(args, n, "substring: not a string", "substring: bad index");
}

static minim_obj prim_string_copy(minim_obj *args, size_t n) {
    return copy_range(args, n, "string-copy: not a string", "string-copy: bad index");
}

// Copies the chars of the string args[2] in the range args[3] and args[4] into
// the string args[0] at args[1].
static minim_obj prim_string_copy_to(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string-copy!: not a string", args[0]);
    check_mutable(args[0], "string-copy!: immutable string");
    size_t len = minim_string_length(args[0]);
    size_t at = bound_arg(args[1], len, "string-copy!: bad index");
    check(minim_is_string(args[2]), "string-copy!: not a string", args[2]);
    range r = range_args(args, n, 3, minim_string_length(args[2]), "string-copy!: bad index");
    check(r.end - r.start <= len - at, "string-copy!: bad index", args[1]);
    memmove(minim_string_chars(args[0]) + at, minim_string_chars(args[2]) + r.start,
            (r.end - r.start) * sizeof(uint32_t));
    return MINIM_VOID;
}

// Sets the chars of the string args[0] in the range args[2] and args[3] to args[1].
static minim_obj prim_string_fill(minim_obj *args, size_t n) {
    check(minim_is_string(args[0]), "string-fill!: not a string", args[0]);
    check_mutable(args[0], "string-fill!: immutable string");
    uint32_t c = (uint32_t)char_arg(args[1], "string-fill!: not a char");
    range r = range_args(args, n, 2, minim_string_length(args[0]), "string-fill!: bad index");
    for (size_t i = r.start; i < r.end; i++)
        minim_string_set(args[0], i, c);
    return MINIM_VOID;
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

// The full case folding of a string, a code point at a time.
typedef struct folding {
    const uint32_t *s;
    size_t n;
    size_t i;                                 // next code point of `s` to fold
    uint32_t buf[MINIM_UNICODE_MAX_MAPPING];  // the last one's folding
    size_t len;                               // of `buf`
    size_t pos;                               // next code point of `buf`
} folding;

// The next code point of the folding `f`, or -1 at its end.
static int32_t next_folded(folding *f) {
    if (f->pos == f->len) {
        if (f->i == f->n)
            return -1;
        f->len = minim_unicode_full_foldcase(f->s, f->n, f->i++, f->buf);
        f->pos = 0;
    }
    return (int32_t)f->buf[f->pos++];
}

// Compares the full case foldings of `a` and `b` by code point.
static int string_compare_ci(minim_obj a, minim_obj b) {
    folding fa = {.s = minim_string_chars(a), .n = minim_string_length(a)};
    folding fb = {.s = minim_string_chars(b), .n = minim_string_length(b)};
    for (;;) {
        int32_t ca = next_folded(&fa);
        int32_t cb = next_folded(&fb);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca < 0)
            return 0;
    }
}

// Whether every adjacent pair of strings satisfies `cmp`, applied to their
// three-way comparison by `order` and 0; raises `msg` on a non-string.
static minim_obj compare_strings(minim_obj *args, size_t n, int (*order)(minim_obj, minim_obj),
                                 bool (*cmp)(intptr_t, intptr_t), const char *msg) {
    bool result = true;
    for (size_t i = 0; i < n; i++) {
        check(minim_is_string(args[i]), msg, args[i]);
        if (i > 0 && !cmp(order(args[i - 1], args[i]), 0))
            result = false;
    }
    return boolean(result);
}

static minim_obj prim_string_eq(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare, eq, "string=?: not a string");
}

static minim_obj prim_string_lt(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare, lt, "string<?: not a string");
}

static minim_obj prim_string_gt(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare, gt, "string>?: not a string");
}

static minim_obj prim_string_le(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare, le, "string<=?: not a string");
}

static minim_obj prim_string_ge(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare, ge, "string>=?: not a string");
}

static minim_obj prim_string_ci_eq(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare_ci, eq, "string-ci=?: not a string");
}

static minim_obj prim_string_ci_lt(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare_ci, lt, "string-ci<?: not a string");
}

static minim_obj prim_string_ci_gt(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare_ci, gt, "string-ci>?: not a string");
}

static minim_obj prim_string_ci_le(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare_ci, le, "string-ci<=?: not a string");
}

static minim_obj prim_string_ci_ge(minim_obj *args, size_t n) {
    return compare_strings(args, n, string_compare_ci, ge, "string-ci>=?: not a string");
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
    {"symbol=?", prim_symbol_eq, 1, MINIM_VARIADIC},
    {"string-length", prim_string_length, 1, 1},
    {"string-ref", prim_string_ref, 2, 2},
    {"string?", prim_is_string, 1, 1},
    {"make-string", prim_make_string, 1, 2},
    {"string-set!", prim_string_set, 3, 3},
    {"string-append", prim_string_append, 0, MINIM_VARIADIC},
    {"substring", prim_substring, 3, 3},
    {"string", prim_string, 0, MINIM_VARIADIC},
    {"string-copy", prim_string_copy, 1, 3},
    {"string-copy!", prim_string_copy_to, 3, 5},
    {"string-fill!", prim_string_fill, 2, 4},
    {"string=?", prim_string_eq, 1, MINIM_VARIADIC},
    {"string<?", prim_string_lt, 1, MINIM_VARIADIC},
    {"string>?", prim_string_gt, 1, MINIM_VARIADIC},
    {"string<=?", prim_string_le, 1, MINIM_VARIADIC},
    {"string>=?", prim_string_ge, 1, MINIM_VARIADIC},
    {"string-ci=?", prim_string_ci_eq, 1, MINIM_VARIADIC},
    {"string-ci<?", prim_string_ci_lt, 1, MINIM_VARIADIC},
    {"string-ci>?", prim_string_ci_gt, 1, MINIM_VARIADIC},
    {"string-ci<=?", prim_string_ci_le, 1, MINIM_VARIADIC},
    {"string-ci>=?", prim_string_ci_ge, 1, MINIM_VARIADIC},
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

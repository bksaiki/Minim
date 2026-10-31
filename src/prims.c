/**
 * @file
 * Primitive procedures.
 */

#include "prims.h"

#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "minim.h"

static minim_obj boolean(bool b) {
    return b ? MINIM_TRUE : MINIM_FALSE;
}

// Raises `msg` with irritant `x` unless `ok`.
static void check(bool ok, const char *msg, minim_obj x) {
    if (!ok)
        minim_error(msg, x);
}

// Value of the index `x` into a sequence of length `len`; raises `msg`
// otherwise.
static size_t index_arg(minim_obj x, size_t len, const char *msg) {
    check(minim_is_fixnum(x) && minim_fixnum_value(x) >= 0
          && (size_t)minim_fixnum_value(x) < len, msg, x);
    return (size_t)minim_fixnum_value(x);
}

// Value of the position `x` in `[0, max]`, for ranges; raises `msg` otherwise.
static size_t bound_arg(minim_obj x, size_t max, const char *msg) {
    check(minim_is_fixnum(x) && minim_fixnum_value(x) >= 0
          && (size_t)minim_fixnum_value(x) <= max, msg, x);
    return (size_t)minim_fixnum_value(x);
}

// Value of the fixnum `x`; raises `msg` otherwise.
static intptr_t fixnum_arg(minim_obj x, const char *msg) {
    check(minim_is_fixnum(x), msg, x);
    return minim_fixnum_value(x);
}

// Code of the char `x`; raises `msg` otherwise.
static intptr_t char_arg(minim_obj x, const char *msg) {
    check(minim_is_char(x), msg, x);
    return minim_char_value(x);
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

static minim_obj prim_quotient(minim_obj *args, size_t n) {
    (void)n;
    intptr_t a = fixnum_arg(args[0], "quotient: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "quotient: not a fixnum");
    check(b != 0, "quotient: division by zero", args[1]);
    return fixnum_result(a / b, "quotient: fixnum overflow");
}

static minim_obj prim_remainder(minim_obj *args, size_t n) {
    (void)n;
    intptr_t a = fixnum_arg(args[0], "remainder: not a fixnum");
    intptr_t b = fixnum_arg(args[1], "remainder: not a fixnum");
    check(b != 0, "remainder: division by zero", args[1]);
    return minim_fixnum(a % b);
}

// Whether the keys of every adjacent pair of arguments satisfy `cmp`.  `key`
// extracts a key from an argument, raising `msg` on the wrong type; every
// argument is checked.
static minim_obj compare(minim_obj *args, size_t n, intptr_t (*key)(minim_obj, const char *),
                         bool (*cmp)(intptr_t, intptr_t), const char *msg) {
    bool result = true;
    intptr_t prev = 0;
    for (size_t i = 0; i < n; i++) {
        intptr_t v = key(args[i], msg);
        if (i > 0 && !cmp(prev, v))
            result = false;
        prev = v;
    }
    return boolean(result);
}

static bool eq(intptr_t a, intptr_t b) { return a == b; }
static bool lt(intptr_t a, intptr_t b) { return a < b; }
static bool gt(intptr_t a, intptr_t b) { return a > b; }
static bool le(intptr_t a, intptr_t b) { return a <= b; }
static bool ge(intptr_t a, intptr_t b) { return a >= b; }

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

static minim_obj prim_is_char(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_char(args[0]));
}

static minim_obj prim_char_to_integer(minim_obj *args, size_t n) {
    (void)n;
    return minim_fixnum(char_arg(args[0], "char->integer: not a char"));
}

static minim_obj prim_integer_to_char(minim_obj *args, size_t n) {
    (void)n;
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

static minim_obj prim_is_fixnum(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_fixnum(args[0]));
}

static minim_obj prim_is_zero(minim_obj *args, size_t n) {
    (void)n;
    return boolean(fixnum_arg(args[0], "zero?: not a fixnum") == 0);
}

static minim_obj prim_cons(minim_obj *args, size_t n) {
    (void)n;
    return minim_cons(args[0], args[1]);
}

static minim_obj prim_car(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_pair(args[0]), "car: not a pair", args[0]);
    return minim_car(args[0]);
}

static minim_obj prim_cdr(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_pair(args[0]), "cdr: not a pair", args[0]);
    return minim_cdr(args[0]);
}

static minim_obj prim_set_car(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_pair(args[0]), "set-car!: not a pair", args[0]);
    minim_set_car(args[0], args[1]);
    return MINIM_VOID;
}

static minim_obj prim_set_cdr(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_pair(args[0]), "set-cdr!: not a pair", args[0]);
    minim_set_cdr(args[0], args[1]);
    return MINIM_VOID;
}

static minim_obj prim_is_pair(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_pair(args[0]));
}

static minim_obj prim_is_null(minim_obj *args, size_t n) {
    (void)n;
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
    (void)n;
    intptr_t len = 0;
    minim_obj x = args[0];
    for (; minim_is_pair(x); x = minim_cdr(x))
        len++;
    check(minim_is_null(x), "length: not a list", args[0]);
    return minim_fixnum(len);
}

// Applied by the evaluator, which owns the value stack; never called.
minim_obj minim_prim_apply(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    abort();
}

// Applied by the evaluator, which evaluates the file's forms; never called.
minim_obj minim_prim_load(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    abort();
}

static minim_obj prim_eq(minim_obj *args, size_t n) {
    (void)n;
    return boolean(args[0] == args[1]);
}

// Pairs of values `equal?` has yet to compare, as `a0, b0, a1, b1, ...`.  It
// starts in `small` and moves to the C heap if it outgrows it.
typedef struct pending {
    minim_obj *items;
    size_t len;
    size_t cap;
    minim_obj small[64];
} pending;

// Queues `a` and `b` for comparison unless they are the same object.
static void pending_push(pending *p, minim_obj a, minim_obj b) {
    if (a == b)
        return;
    if (p->len + 2 > p->cap) {
        p->cap *= 2;
        minim_obj *items = p->items == p->small
            ? malloc(p->cap * sizeof(minim_obj))
            : realloc(p->items, p->cap * sizeof(minim_obj));
        if (items == NULL)
            minim_out_of_memory();
        if (p->items == p->small)
            memcpy(items, p->small, p->len * sizeof(minim_obj));
        p->items = items;
    }
    p->items[p->len++] = a;
    p->items[p->len++] = b;
}

// Compares with an explicit stack, so deep structure can't overflow the C
// stack.  Nothing here allocates on the heap, so queued values stay valid,
// and nothing raises, so the stack is always freed.
static bool is_equal(minim_obj a, minim_obj b) {
    pending p = {.cap = sizeof(p.small) / sizeof(p.small[0])};
    p.items = p.small;
    pending_push(&p, a, b);

    bool result = true;
    while (result && p.len > 0) {
        b = p.items[--p.len];
        a = p.items[--p.len];
        if (minim_is_pair(a) && minim_is_pair(b)) {
            pending_push(&p, minim_cdr(a), minim_cdr(b));
            pending_push(&p, minim_car(a), minim_car(b));
        } else if (minim_is_vector(a) && minim_is_vector(b)) {
            result = minim_vector_length(a) == minim_vector_length(b);
            for (size_t i = 0; result && i < minim_vector_length(a); i++)
                pending_push(&p, minim_vector_ref(a, i), minim_vector_ref(b, i));
        } else if (minim_is_string(a) && minim_is_string(b)) {
            result = minim_string_equal(a, b);
        } else {
            result = false;  // distinct, and not both pairs, vectors, or strings
        }
    }

    if (p.items != p.small)
        free(p.items);
    return result;
}

static minim_obj prim_equal(minim_obj *args, size_t n) {
    (void)n;
    return boolean(is_equal(args[0], args[1]));
}

// Values `equal-hash` visits, at most; the rest of the structure is ignored,
// which keeps it O(1) and terminating on cyclic data.
enum { HASH_BUDGET = 64 };

// Kind markers, so different kinds with the same contents hash differently
enum { HASH_PAIR = 1, HASH_VECTOR, HASH_SYMBOL, HASH_CLOSURE };

// Hash agreeing with `equal?`: equal values visit the same nodes in the same
// order, so they consume the budget identically.
static uint64_t equal_hash(minim_obj x, int *budget) {
    if (--*budget < 0)
        return 0;
    if (minim_is_string(x))
        return minim_string_hash(x);
    if (minim_is_symbol(x))
        return minim_hash_mix(minim_string_hash(minim_symbol_name(x)), HASH_SYMBOL);
    if (minim_is_primitive(x))
        return minim_hash_mix(MINIM_HASH_INIT, (uintptr_t)minim_primitive_fn(x));
    if (minim_is_closure(x))
        return HASH_CLOSURE;  // closures are equal? only when eq?
    if (minim_is_pair(x)) {
        uint64_t h = minim_hash_mix(MINIM_HASH_INIT, HASH_PAIR);
        h = minim_hash_mix(h, equal_hash(minim_car(x), budget));
        return minim_hash_mix(h, equal_hash(minim_cdr(x), budget));
    }
    if (minim_is_vector(x)) {
        uint64_t h = minim_hash_mix(MINIM_HASH_INIT, HASH_VECTOR);
        h = minim_hash_mix(h, minim_vector_length(x));
        for (size_t i = 0; i < minim_vector_length(x) && *budget > 0; i++)
            h = minim_hash_mix(h, equal_hash(minim_vector_ref(x, i), budget));
        return h;
    }
    return minim_hash_mix(MINIM_HASH_INIT, x);  // immediates
}

// Non-negative fixnum for the hash `h`
static minim_obj hash_result(uint64_t h) {
    return minim_fixnum((intptr_t)(h & (uint64_t)MINIM_FIXNUM_MAX));
}

static minim_obj prim_equal_hash(minim_obj *args, size_t n) {
    (void)n;
    int budget = HASH_BUDGET;
    return hash_result(equal_hash(args[0], &budget));
}

static minim_obj prim_string_hash(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_string(args[0]), "string-hash: not a string", args[0]);
    return hash_result(minim_string_hash(args[0]));
}

static minim_obj prim_not(minim_obj *args, size_t n) {
    (void)n;
    return boolean(args[0] == MINIM_FALSE);
}

static minim_obj prim_is_symbol(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_symbol(args[0]));
}

static minim_obj prim_is_procedure(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_procedure(args[0]));
}

static minim_obj prim_is_boolean(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_boolean(args[0]));
}

static minim_obj prim_vector(minim_obj *args, size_t n) {
    minim_obj v = minim_make_vector(n, MINIM_NIL);
    for (size_t i = 0; i < n; i++)
        minim_vector_set(v, i, args[i]);
    return v;
}

static minim_obj prim_make_vector(minim_obj *args, size_t n) {
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= 0,
          "make-vector: bad length", args[0]);
    minim_obj fill = n == 2 ? args[1] : minim_fixnum(0);
    return minim_make_vector((size_t)minim_fixnum_value(args[0]), fill);
}

static minim_obj prim_vector_ref(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_vector(args[0]), "vector-ref: not a vector", args[0]);
    size_t i = index_arg(args[1], minim_vector_length(args[0]), "vector-ref: bad index");
    return minim_vector_ref(args[0], i);
}

static minim_obj prim_vector_set(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_vector(args[0]), "vector-set!: not a vector", args[0]);
    size_t i = index_arg(args[1], minim_vector_length(args[0]), "vector-set!: bad index");
    minim_vector_set(args[0], i, args[2]);
    return MINIM_VOID;
}

static minim_obj prim_vector_length(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_vector(args[0]), "vector-length: not a vector", args[0]);
    return minim_fixnum((intptr_t)minim_vector_length(args[0]));
}

static minim_obj prim_is_vector(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_vector(args[0]));
}

static minim_obj prim_string_length(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_string(args[0]), "string-length: not a string", args[0]);
    return minim_fixnum((intptr_t)minim_string_length(args[0]));
}

static minim_obj prim_string_ref(minim_obj *args, size_t n) {
    (void)n;
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
    (void)n;
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
    (void)n;
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
    (void)n;
    check(minim_is_string(args[0]), "string->list: not a string", args[0]);
    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = minim_string_length(args[0]); i > 0; i--)
        lst = minim_cons(minim_char(minim_string_ref(args[0], i - 1)), lst);
    MINIM_RETURN(lst);
}

static minim_obj prim_list_to_string(minim_obj *args, size_t n) {
    (void)n;
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
    (void)n;
    return boolean(minim_is_string(args[0]));
}

// Returns the symbol's immutable name itself.
static minim_obj prim_symbol_to_string(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_symbol(args[0]), "symbol->string: not a symbol", args[0]);
    return minim_symbol_name(args[0]);
}

static minim_obj prim_string_to_symbol(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_string(args[0]), "string->symbol: not a string", args[0]);
    return minim_intern(args[0]);
}

static minim_obj prim_write(minim_obj *args, size_t n) {
    (void)n;
    minim_write(stdout, args[0]);
    return MINIM_VOID;
}

static minim_obj prim_display(minim_obj *args, size_t n) {
    (void)n;
    minim_display(stdout, args[0]);
    return MINIM_VOID;
}

static minim_obj prim_newline(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    putchar('\n');
    return MINIM_VOID;
}

static minim_obj prim_collect(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    minim_gc_collect();
    return MINIM_VOID;
}

static minim_obj prim_collect_nursery_bytes(minim_obj *args, size_t n) {
    minim_gc_policy policy = minim_gc_get_policy();
    if (n == 0)
        return minim_fixnum((intptr_t)policy.nursery_bytes);
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) > 0,
          "collect-nursery-bytes: not a positive fixnum", args[0]);
    policy.nursery_bytes = (size_t)minim_fixnum_value(args[0]);
    minim_gc_set_policy(policy);
    return MINIM_VOID;
}

static minim_obj prim_collect_generation_radix(minim_obj *args, size_t n) {
    minim_gc_policy policy = minim_gc_get_policy();
    if (n == 0)
        return minim_fixnum((intptr_t)policy.radix);
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= 0,
          "collect-generation-radix: not a nonnegative fixnum", args[0]);
    policy.radix = (size_t)minim_fixnum_value(args[0]);
    minim_gc_set_policy(policy);
    return MINIM_VOID;
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
    define_primitive(">", prim_gt, 1, MINIM_VARIADIC);
    define_primitive("<=", prim_le, 1, MINIM_VARIADIC);
    define_primitive(">=", prim_ge, 1, MINIM_VARIADIC);
    define_primitive("quotient", prim_quotient, 2, 2);
    define_primitive("remainder", prim_remainder, 2, 2);
    define_primitive("fixnum?", prim_is_fixnum, 1, 1);
    define_primitive("zero?", prim_is_zero, 1, 1);
    define_primitive("char?", prim_is_char, 1, 1);
    define_primitive("char->integer", prim_char_to_integer, 1, 1);
    define_primitive("integer->char", prim_integer_to_char, 1, 1);
    define_primitive("char=?", prim_char_eq, 1, MINIM_VARIADIC);
    define_primitive("char<?", prim_char_lt, 1, MINIM_VARIADIC);
    define_primitive("char>?", prim_char_gt, 1, MINIM_VARIADIC);
    define_primitive("char<=?", prim_char_le, 1, MINIM_VARIADIC);
    define_primitive("char>=?", prim_char_ge, 1, MINIM_VARIADIC);
    define_primitive("cons", prim_cons, 2, 2);
    define_primitive("car", prim_car, 1, 1);
    define_primitive("cdr", prim_cdr, 1, 1);
    define_primitive("set-car!", prim_set_car, 2, 2);
    define_primitive("set-cdr!", prim_set_cdr, 2, 2);
    define_primitive("pair?", prim_is_pair, 1, 1);
    define_primitive("null?", prim_is_null, 1, 1);
    define_primitive("list", prim_list, 0, MINIM_VARIADIC);
    define_primitive("length", prim_length, 1, 1);
    define_primitive("apply", minim_prim_apply, 2, MINIM_VARIADIC);
    define_primitive("load", minim_prim_load, 1, 1);
    define_primitive("eq?", prim_eq, 2, 2);
    define_primitive("eqv?", prim_eq, 2, 2);
    define_primitive("equal?", prim_equal, 2, 2);
    define_primitive("equal-hash", prim_equal_hash, 1, 1);
    define_primitive("string-hash", prim_string_hash, 1, 1);
    define_primitive("not", prim_not, 1, 1);
    define_primitive("symbol?", prim_is_symbol, 1, 1);
    define_primitive("procedure?", prim_is_procedure, 1, 1);
    define_primitive("boolean?", prim_is_boolean, 1, 1);
    define_primitive("vector", prim_vector, 0, MINIM_VARIADIC);
    define_primitive("make-vector", prim_make_vector, 1, 2);
    define_primitive("vector-ref", prim_vector_ref, 2, 2);
    define_primitive("vector-set!", prim_vector_set, 3, 3);
    define_primitive("vector-length", prim_vector_length, 1, 1);
    define_primitive("vector?", prim_is_vector, 1, 1);
    define_primitive("string-length", prim_string_length, 1, 1);
    define_primitive("string-ref", prim_string_ref, 2, 2);
    define_primitive("string?", prim_is_string, 1, 1);
    define_primitive("make-string", prim_make_string, 1, 2);
    define_primitive("string-set!", prim_string_set, 3, 3);
    define_primitive("string-append", prim_string_append, 0, MINIM_VARIADIC);
    define_primitive("substring", prim_substring, 3, 3);
    define_primitive("string=?", prim_string_eq, 1, MINIM_VARIADIC);
    define_primitive("string<?", prim_string_lt, 1, MINIM_VARIADIC);
    define_primitive("string>?", prim_string_gt, 1, MINIM_VARIADIC);
    define_primitive("string<=?", prim_string_le, 1, MINIM_VARIADIC);
    define_primitive("string>=?", prim_string_ge, 1, MINIM_VARIADIC);
    define_primitive("string->list", prim_string_to_list, 1, 1);
    define_primitive("list->string", prim_list_to_string, 1, 1);
    define_primitive("symbol->string", prim_symbol_to_string, 1, 1);
    define_primitive("string->symbol", prim_string_to_symbol, 1, 1);
    define_primitive("write", prim_write, 1, 1);
    define_primitive("display", prim_display, 1, 1);
    define_primitive("newline", prim_newline, 0, 0);
    define_primitive("collect", prim_collect, 0, 0);
    define_primitive("collect-nursery-bytes", prim_collect_nursery_bytes, 0, 1);
    define_primitive("collect-generation-radix", prim_collect_generation_radix, 0, 1);
}

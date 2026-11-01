/**
 * @file
 * Primitive procedures.
 */

#include "prims.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "gc.h"
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
        (void)n;                                                       \
        return cxr(args[0], #path, "c" #path "r: not a pair");         \
    }
CXR(aa)
CXR(ad)
CXR(da)
CXR(dd)
#undef CXR

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

// Applied by the evaluator, which returns the values; never called.
minim_obj minim_prim_values(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    abort();
}

// Applied by the evaluator, which passes the producer's values to the consumer;
// never called.
minim_obj minim_prim_call_with_values(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    abort();
}

// Applied by the evaluator, which captures the continuation; never called.
minim_obj minim_prim_call_cc(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    abort();
}

// The `dynamic-wind` calls in progress, for the prelude's `call/cc`.
static minim_obj prim_winders(minim_obj *args, size_t n) {
    (void)args;
    (void)n;
    return minim_gc.winders;
}

static minim_obj prim_set_winders(minim_obj *args, size_t n) {
    (void)n;
    minim_gc.winders = args[0];
    return MINIM_VOID;
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
    if (minim_is_closure(x) || minim_is_continuation(x) || minim_is_error_object(x))
        return HASH_CLOSURE;  // equal? only when eq?
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

// The elements of the vector args[0] from args[1] (default 0) to args[2]
// (default its length), as a list.
static minim_obj prim_vector_to_list(minim_obj *args, size_t n) {
    check(minim_is_vector(args[0]), "vector->list: not a vector", args[0]);
    size_t len = minim_vector_length(args[0]);
    size_t start = n > 1 ? bound_arg(args[1], len, "vector->list: bad index") : 0;
    size_t end = n > 2 ? bound_arg(args[2], len, "vector->list: bad index") : len;
    check(start <= end, "vector->list: bad index", args[1]);

    minim_obj lst = MINIM_NIL;
    MINIM_FRAME(&lst);
    for (size_t i = end; i > start; i--)
        lst = minim_cons(minim_vector_ref(args[0], i - 1), lst);
    MINIM_RETURN(lst);
}

// A new vector of the elements of the proper list args[0].
static minim_obj prim_list_to_vector(minim_obj *args, size_t n) {
    (void)n;
    size_t len = 0;
    minim_obj x = args[0];
    for (; minim_is_pair(x); x = minim_cdr(x))
        len++;
    check(minim_is_null(x), "list->vector: not a list", args[0]);

    minim_obj v = minim_make_vector(len, MINIM_NIL);
    x = args[0];  // the collection may have moved it
    for (size_t i = 0; i < len; i++, x = minim_cdr(x))
        minim_vector_set(v, i, minim_car(x));
    return v;
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

// Returns the collector setting at `offset` in minim_gc_config, or sets it to
// args[0], a fixnum from `min` to `max`.
static minim_obj config_setting(minim_obj *args, size_t n, size_t offset, intptr_t min,
                                intptr_t max, const char *msg) {
    minim_gc_config config = minim_gc_get_config();
    size_t *field = (size_t *)((char *)&config + offset);
    if (n == 0)
        return minim_fixnum((intptr_t)*field);
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= min
          && minim_fixnum_value(args[0]) <= max, msg, args[0]);
    *field = (size_t)minim_fixnum_value(args[0]);
    minim_gc_set_config(config);
    return MINIM_VOID;
}

static minim_obj prim_collect_nursery_bytes(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, nursery_bytes), 1, MINIM_FIXNUM_MAX,
                          "collect-nursery-bytes: not a positive fixnum");
}

static minim_obj prim_collect_generation_radix(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, radix), 1, MINIM_FIXNUM_MAX,
                          "collect-generation-radix: not a positive fixnum");
}

static minim_obj prim_collect_maximum_generation(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, max_generation), 1,
                          MINIM_MAX_GENERATION, "collect-maximum-generation: bad generation");
}

// Aliases are uninterned symbols whose value is `#(alias_tag id env)`.
static minim_obj alias_tag;

// The `#(alias_tag id env)` of the alias `x`, or `#f` if `x` isn't one.
static minim_obj alias_info(minim_obj x) {
    if (!minim_is_symbol(x))
        return MINIM_FALSE;
    minim_obj v = minim_symbol_value(x);
    return minim_is_vector(v) && minim_vector_length(v) == 3 && minim_vector_ref(v, 0) == alias_tag
        ? v : MINIM_FALSE;
}

static minim_obj prim_gensym(minim_obj *args, size_t n) {
    static uintptr_t count;
    if (n == 1) {
        check(minim_is_string(args[0]), "gensym: not a string", args[0]);
        return minim_make_uninterned(args[0]);
    }
    char name[32];
    snprintf(name, sizeof(name), "g%" PRIuPTR, count++);
    return minim_make_uninterned(minim_string_from_cstr(name));
}

static minim_obj prim_make_alias(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_symbol(args[0]), "%make-alias: not a symbol", args[0]);
    minim_obj alias = minim_make_uninterned(minim_symbol_name(args[0]));
    MINIM_FRAME(&alias);
    minim_obj info = minim_make_vector(3, alias_tag);
    minim_vector_set(info, 1, args[0]);
    minim_vector_set(info, 2, args[1]);
    minim_set_symbol_value(alias, info);
    MINIM_RETURN(alias);
}

static minim_obj prim_alias_original(minim_obj *args, size_t n) {
    (void)n;
    minim_obj info = alias_info(args[0]);
    return info == MINIM_FALSE ? info : minim_vector_ref(info, 1);
}

static minim_obj prim_alias_env(minim_obj *args, size_t n) {
    (void)n;
    minim_obj info = alias_info(args[0]);
    return info == MINIM_FALSE ? info : minim_vector_ref(info, 2);
}

static minim_obj prim_is_bound(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_symbol(args[0]), "%bound?: not a symbol", args[0]);
    return boolean(minim_symbol_value(args[0]) != MINIM_UNBOUND
                   && alias_info(args[0]) == MINIM_FALSE);
}

static minim_obj prim_make_error_object(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_string(args[0]), "%make-error-object: not a string", args[0]);
    check(minim_is_null(args[1]) || minim_is_pair(args[1]),
          "%make-error-object: not a list", args[1]);
    return minim_make_error_object(MINIM_ERROR_GENERAL, args[0], args[1]);
}

static minim_obj prim_is_error_object(minim_obj *args, size_t n) {
    (void)n;
    return boolean(minim_is_error_object(args[0]));
}

static minim_obj prim_error_object_message(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_error_object(args[0]), "error-object-message: not an error object", args[0]);
    return minim_error_object_message(args[0]);
}

static minim_obj prim_error_object_irritants(minim_obj *args, size_t n) {
    (void)n;
    check(minim_is_error_object(args[0]), "error-object-irritants: not an error object",
          args[0]);
    return minim_error_object_irritants(args[0]);
}

static bool is_error_of_kind(minim_obj x, minim_error_kind kind) {
    return minim_is_error_object(x) && minim_error_object_kind(x) == kind;
}

static minim_obj prim_is_read_error(minim_obj *args, size_t n) {
    (void)n;
    return boolean(is_error_of_kind(args[0], MINIM_ERROR_READ));
}

static minim_obj prim_is_file_error(minim_obj *args, size_t n) {
    (void)n;
    return boolean(is_error_of_kind(args[0], MINIM_ERROR_FILE));
}

// Raises the message args[0] with the irritants args[1]: void if none, the
// one if single, else the list.
static minim_obj prim_syntax_error(minim_obj *args, size_t n) {
    (void)n;
    static char *message;  // must outlive the catch; replaced by the next one
    minim_obj rest = args[1];
    check(minim_is_string(args[0]), "syntax-error: not a string", args[0]);
    check(minim_is_null(rest) || minim_is_pair(rest), "syntax-error: not a list", rest);
    free(message);
    message = minim_string_to_utf8(args[0]);
    if (minim_is_null(rest))
        minim_error(message, MINIM_VOID);
    minim_error(message, minim_is_null(minim_cdr(rest)) ? minim_car(rest) : rest);
}

static void define_primitive(const char *name, minim_prim_fn fn, intptr_t min, intptr_t max) {
    minim_obj sym = minim_intern_cstr(name);
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, fn, min, max);
    minim_set_symbol_value(sym, p);
    MINIM_RETURN_VOID;
}

void minim_prims_init(void) {
    alias_tag = minim_make_uninterned(minim_string_from_cstr("alias"));
    minim_gc_add_global(&alias_tag);

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
    define_primitive("number->string", prim_number_to_string, 1, 2);
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
    define_primitive("caar", prim_caar, 1, 1);
    define_primitive("cadr", prim_cadr, 1, 1);
    define_primitive("cdar", prim_cdar, 1, 1);
    define_primitive("cddr", prim_cddr, 1, 1);
    define_primitive("set-car!", prim_set_car, 2, 2);
    define_primitive("set-cdr!", prim_set_cdr, 2, 2);
    define_primitive("pair?", prim_is_pair, 1, 1);
    define_primitive("null?", prim_is_null, 1, 1);
    define_primitive("list", prim_list, 0, MINIM_VARIADIC);
    define_primitive("length", prim_length, 1, 1);
    define_primitive("apply", minim_prim_apply, 2, MINIM_VARIADIC);
    define_primitive("load", minim_prim_load, 1, 1);
    define_primitive("values", minim_prim_values, 0, MINIM_VARIADIC);
    define_primitive("call-with-values", minim_prim_call_with_values, 2, 2);
    define_primitive("%call/cc", minim_prim_call_cc, 1, 1);
    define_primitive("%winders", prim_winders, 0, 0);
    define_primitive("%set-winders!", prim_set_winders, 1, 1);
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
    define_primitive("vector->list", prim_vector_to_list, 1, 3);
    define_primitive("list->vector", prim_list_to_vector, 1, 1);
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
    define_primitive("gensym", prim_gensym, 0, 1);
    define_primitive("%make-alias", prim_make_alias, 2, 2);
    define_primitive("%alias-original", prim_alias_original, 1, 1);
    define_primitive("%alias-env", prim_alias_env, 1, 1);
    define_primitive("%bound?", prim_is_bound, 1, 1);
    define_primitive("%syntax-error", prim_syntax_error, 2, 2);
    define_primitive("%make-error-object", prim_make_error_object, 2, 2);
    define_primitive("error-object?", prim_is_error_object, 1, 1);
    define_primitive("error-object-message", prim_error_object_message, 1, 1);
    define_primitive("error-object-irritants", prim_error_object_irritants, 1, 1);
    define_primitive("read-error?", prim_is_read_error, 1, 1);
    define_primitive("file-error?", prim_is_file_error, 1, 1);
    define_primitive("write", prim_write, 1, 1);
    define_primitive("display", prim_display, 1, 1);
    define_primitive("newline", prim_newline, 0, 0);
    define_primitive("collect", prim_collect, 0, 0);
    define_primitive("collect-nursery-bytes", prim_collect_nursery_bytes, 0, 1);
    define_primitive("collect-generation-radix", prim_collect_generation_radix, 0, 1);
    define_primitive("collect-maximum-generation", prim_collect_maximum_generation, 0, 1);
}

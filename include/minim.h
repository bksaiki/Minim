/**
 * @file
 * Public API: tagged values, garbage collection, and object operations.
 */

#ifndef MINIM_H
#define MINIM_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * A tagged Scheme value.  The low 3 bits are the primary tag; heap objects
 * are 16-byte aligned, so pointers carry the tag in their low bits.
 */
typedef uintptr_t minim_obj;

static_assert(sizeof(void *) == 8, "Minim requires 64-bit pointers");
static_assert(sizeof(minim_obj) == 8, "minim_obj must be 64 bits");
static_assert((-8 >> 3) == -1, "Minim requires arithmetic right shift");

/** Primary tags: the low 3 bits of a minim_obj. */
enum {
    MINIM_TAG_BITS = 3,
    MINIM_TAG_MASK = 0x7,

    MINIM_TAG_FIXNUM = 0x0,
    MINIM_TAG_PAIR = 0x1,
    MINIM_TAG_SYMBOL = 0x3,
    MINIM_TAG_IMMEDIATE = 0x6,
    MINIM_TAG_TYPED = 0x7,
};

/** Immediates: the low byte is the type, ending in MINIM_TAG_IMMEDIATE. */
enum {
    MINIM_IMMEDIATE_MASK = 0xFF,

    MINIM_FALSE = 0x06,
    MINIM_TRUE = 0x0E,
    MINIM_TYPE_CHAR = 0x16,
    MINIM_UNBOUND = 0x1E,
    MINIM_NIL = 0x26,
    MINIM_VOID = 0x2E,
    MINIM_EOF = 0x36,
    MINIM_FORWARD = 0x3E,  ///< GC forwarding marker, never a user value

    MINIM_CHAR_SHIFT = 8,
};

/** Typed-object headers: `(length << 4) | (immutable << 3) | type`. */
enum {
    MINIM_HEADER_TYPE_MASK = 0x7,
    MINIM_HEADER_IMMUTABLE = 0x8,
    MINIM_HEADER_LENGTH_SHIFT = 4,

    MINIM_TYPE_VECTOR = 0x0,
    MINIM_TYPE_STRING = 0x2,
};

/** Object sizes in bytes, before rounding to the allocation unit. */
enum {
    MINIM_PAIR_SIZE = 2 * sizeof(minim_obj),
    MINIM_SYMBOL_SIZE = 2 * sizeof(minim_obj),
};

/** Size in bytes of a vector of length `n`. */
static inline size_t minim_vector_size(size_t n) {
    return (1 + n) * sizeof(minim_obj);
}

/** Size in bytes of a string of length `n`. */
static inline size_t minim_string_size(size_t n) {
    return sizeof(minim_obj) + n * sizeof(uint32_t);
}

// Fixnums: 61-bit signed integers stored as n << 3

/** Largest fixnum. */
#define MINIM_FIXNUM_MAX ((intptr_t)(((uintptr_t)1 << 60) - 1))

/** Smallest fixnum. */
#define MINIM_FIXNUM_MIN (-MINIM_FIXNUM_MAX - 1)

/** Primary tag of `x`. */
static inline minim_obj minim_tag(minim_obj x) {
    return x & MINIM_TAG_MASK;
}

/** Fixnum for `n`, which must be in [MINIM_FIXNUM_MIN, MINIM_FIXNUM_MAX]. */
static inline minim_obj minim_fixnum(intptr_t n) {
    assert(n >= MINIM_FIXNUM_MIN && n <= MINIM_FIXNUM_MAX);
    return (minim_obj)n << MINIM_TAG_BITS;
}

/** Integer value of the fixnum `x`. */
static inline intptr_t minim_fixnum_value(minim_obj x) {
    return (intptr_t)x >> MINIM_TAG_BITS;
}

// Chars: Unicode scalar values stored as (c << 8) | MINIM_TYPE_CHAR

/** Whether `c` is a Unicode scalar value. */
static inline bool minim_is_scalar_value(uint32_t c) {
    return c <= 0x10FFFF && (c < 0xD800 || c > 0xDFFF);
}

/** Char for the scalar value `c`. */
static inline minim_obj minim_char(uint32_t c) {
    assert(minim_is_scalar_value(c));
    return ((minim_obj)c << MINIM_CHAR_SHIFT) | MINIM_TYPE_CHAR;
}

/** Scalar value of the char `x`. */
static inline uint32_t minim_char_value(minim_obj x) {
    return (uint32_t)(x >> MINIM_CHAR_SHIFT);
}

// Typed objects

/** Header for a mutable typed object. */
static inline minim_obj minim_make_header(size_t length, minim_obj type) {
    return ((minim_obj)length << MINIM_HEADER_LENGTH_SHIFT) | type;
}

/** Length field of the typed-object header `h`. */
static inline size_t minim_header_length(minim_obj h) {
    return (size_t)(h >> MINIM_HEADER_LENGTH_SHIFT);
}

/** Type field of the typed-object header `h`. */
static inline minim_obj minim_header_type(minim_obj h) {
    return h & MINIM_HEADER_TYPE_MASK;
}

/** Header word of the typed object `x`. */
static inline minim_obj minim_typed_header(minim_obj x) {
    return *(const minim_obj *)(x - MINIM_TAG_TYPED);
}

// Predicates

/** Whether `x` is a fixnum. */
static inline bool minim_is_fixnum(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_FIXNUM;
}

/** Whether `x` is a pair. */
static inline bool minim_is_pair(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_PAIR;
}

/** Whether `x` is a symbol. */
static inline bool minim_is_symbol(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_SYMBOL;
}

/** Whether `x` is a typed object. */
static inline bool minim_is_typed(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_TYPED;
}

/** Whether `x` is a vector. */
static inline bool minim_is_vector(minim_obj x) {
    return minim_is_typed(x)
        && minim_header_type(minim_typed_header(x)) == MINIM_TYPE_VECTOR;
}

/** Whether `x` is a string. */
static inline bool minim_is_string(minim_obj x) {
    return minim_is_typed(x)
        && minim_header_type(minim_typed_header(x)) == MINIM_TYPE_STRING;
}

/** Whether `x` is a char. */
static inline bool minim_is_char(minim_obj x) {
    return (x & MINIM_IMMEDIATE_MASK) == MINIM_TYPE_CHAR;
}

/** Whether `x` is `#t` or `#f`. */
static inline bool minim_is_boolean(minim_obj x) {
    return x == MINIM_FALSE || x == MINIM_TRUE;
}

/** Whether `x` is the empty list. */
static inline bool minim_is_null(minim_obj x) {
    return x == MINIM_NIL;
}

// Runtime

/** Starts the runtime. */
void minim_init(void);

/** Frees all runtime memory. */
void minim_shutdown(void);

// Garbage collection

/** Root stack height; used by MINIM_FRAME. */
size_t minim_gc_mark(void);

/** Pops the root stack to `mark`; used by MINIM_RETURN. */
void minim_gc_reset(size_t mark);

/** Pushes `n` variables onto the root stack; used by MINIM_PROTECT. */
void minim_gc_protect(minim_obj *const *vars, size_t n);

/**
 * Opens a frame protecting the variables at the given addresses.  Allocation
 * moves objects, so protect every local live across one.  Close with
 * MINIM_RETURN.  Don't nest allocating calls: in `f(a, minim_cons(b, c))`,
 * `a` may be read before the collection.
 */
#define MINIM_FRAME(...)                       \
    size_t minim_frame_ = minim_gc_mark();     \
    MINIM_PROTECT(__VA_ARGS__)

/** Protects more variables until the frame closes. */
#define MINIM_PROTECT(...)                                       \
    minim_gc_protect((minim_obj *[]){__VA_ARGS__},               \
                     sizeof((minim_obj *[]){__VA_ARGS__}) / sizeof(minim_obj *))

/** Closes the frame and returns `e`, evaluated inside it. */
#define MINIM_RETURN(e)                        \
    do {                                       \
        minim_obj minim_result_ = (e);         \
        minim_gc_reset(minim_frame_);          \
        return minim_result_;                  \
    } while (0)

/** Closes the frame and returns from a `void` function. */
#define MINIM_RETURN_VOID                      \
    do {                                       \
        minim_gc_reset(minim_frame_);          \
        return;                                \
    } while (0)

/** Protects a variable for the life of the runtime. */
void minim_gc_add_global(minim_obj *p);

/** Collects garbage now. */
void minim_gc_collect(void);

// Pairs

/** New pair. */
minim_obj minim_cons(minim_obj car, minim_obj cdr);

/** First field of the pair `p`. */
static inline minim_obj minim_car(minim_obj p) {
    assert(minim_is_pair(p));
    return ((const minim_obj *)(p - MINIM_TAG_PAIR))[0];
}

/** Second field of the pair `p`. */
static inline minim_obj minim_cdr(minim_obj p) {
    assert(minim_is_pair(p));
    return ((const minim_obj *)(p - MINIM_TAG_PAIR))[1];
}

/** Sets the first field of the pair `p`. */
static inline void minim_set_car(minim_obj p, minim_obj x) {
    assert(minim_is_pair(p));
    ((minim_obj *)(p - MINIM_TAG_PAIR))[0] = x;
}

/** Sets the second field of the pair `p`. */
static inline void minim_set_cdr(minim_obj p, minim_obj x) {
    assert(minim_is_pair(p));
    ((minim_obj *)(p - MINIM_TAG_PAIR))[1] = x;
}

// Vectors

/** New vector of length `n` with every element `fill`. */
minim_obj minim_make_vector(size_t n, minim_obj fill);

/** Length of the vector `v`. */
static inline size_t minim_vector_length(minim_obj v) {
    assert(minim_is_vector(v));
    return minim_header_length(minim_typed_header(v));
}

/** Element `i` of the vector `v`. */
static inline minim_obj minim_vector_ref(minim_obj v, size_t i) {
    assert(i < minim_vector_length(v));
    return ((const minim_obj *)(v - MINIM_TAG_TYPED))[1 + i];
}

/** Sets element `i` of the vector `v`. */
static inline void minim_vector_set(minim_obj v, size_t i, minim_obj x) {
    assert(i < minim_vector_length(v));
    ((minim_obj *)(v - MINIM_TAG_TYPED))[1 + i] = x;
}

// Strings: arrays of Unicode scalar values

/** New string of length `n` with every char `c`. */
minim_obj minim_make_string(size_t n, uint32_t c);

/** New string from UTF-8; invalid bytes become U+FFFD. */
minim_obj minim_string_from_cstr(const char *s);

/** Length of the string `s`. */
static inline size_t minim_string_length(minim_obj s) {
    assert(minim_is_string(s));
    return minim_header_length(minim_typed_header(s));
}

/** Chars of the string `s`, valid until the next allocation. */
static inline uint32_t *minim_string_chars(minim_obj s) {
    assert(minim_is_string(s));
    return (uint32_t *)(s - MINIM_TAG_TYPED + sizeof(minim_obj));
}

/** Char `i` of the string `s`. */
static inline uint32_t minim_string_ref(minim_obj s, size_t i) {
    assert(i < minim_string_length(s));
    return minim_string_chars(s)[i];
}

/** Sets char `i` of the mutable string `s`. */
static inline void minim_string_set(minim_obj s, size_t i, uint32_t c) {
    assert(i < minim_string_length(s) && minim_is_scalar_value(c));
    assert(!(minim_typed_header(s) & MINIM_HEADER_IMMUTABLE));
    minim_string_chars(s)[i] = c;
}

// Symbols: interned and never freed

/** Symbol named `str`; a new one gets a copy of `str` and is unbound. */
minim_obj minim_intern(minim_obj str);

/** minim_intern with a UTF-8 name. */
minim_obj minim_intern_cstr(const char *s);

/** Name of the symbol `sym` (immutable). */
static inline minim_obj minim_symbol_name(minim_obj sym) {
    assert(minim_is_symbol(sym));
    return ((const minim_obj *)(sym - MINIM_TAG_SYMBOL))[0];
}

/** Value of the symbol `sym`. */
static inline minim_obj minim_symbol_value(minim_obj sym) {
    assert(minim_is_symbol(sym));
    return ((const minim_obj *)(sym - MINIM_TAG_SYMBOL))[1];
}

/** Sets the value of the symbol `sym`. */
static inline void minim_set_symbol_value(minim_obj sym, minim_obj x) {
    assert(minim_is_symbol(sym));
    ((minim_obj *)(sym - MINIM_TAG_SYMBOL))[1] = x;
}

// Errors

/**
 * Jumps to the innermost minim_catch, recording `msg` (which must outlive the
 * catch, e.g. a literal) and `irritant` (MINIM_VOID for none).  With no catch
 * installed, prints the error and aborts.
 */
_Noreturn void minim_error(const char *msg, minim_obj irritant);

/**
 * Calls `fn(arg)`.  Returns `true` and stores its result in `*result`, or
 * returns `false` if it raised an error, restoring the root stack.
 */
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);

/** Message and irritant of the last error. */
void minim_last_error(const char **msg, minim_obj *irritant);

/** Writes the last error to `out` as `error: msg: irritant`. */
void minim_write_error(FILE *out);

// Reading

/** Reads the next datum from `in`, or returns MINIM_EOF.  Raises on bad syntax. */
minim_obj minim_read(FILE *in);

// Printing

/** Writes `x` to `out` in `write` syntax.  Never allocates. */
void minim_write(FILE *out, minim_obj x);

#endif

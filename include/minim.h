#ifndef MINIM_H
#define MINIM_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A tagged Scheme value.  The low 3 bits are the primary tag; heap objects
// are 16-byte aligned, so pointers carry the tag in their low bits.
typedef uintptr_t minim_obj;

static_assert(sizeof(void *) == 8, "Minim requires 64-bit pointers");
static_assert(sizeof(minim_obj) == 8, "minim_obj must be 64 bits");
static_assert((-8 >> 3) == -1, "Minim requires arithmetic right shift");

// Primary tags
enum {
    MINIM_TAG_BITS = 3,
    MINIM_TAG_MASK = 0x7,

    MINIM_TAG_FIXNUM = 0x0,
    MINIM_TAG_PAIR = 0x1,
    MINIM_TAG_SYMBOL = 0x3,
    MINIM_TAG_IMMEDIATE = 0x6,
    MINIM_TAG_TYPED = 0x7,
};

// Immediates: the low byte is the type, ending in MINIM_TAG_IMMEDIATE.
enum {
    MINIM_IMMEDIATE_MASK = 0xFF,

    MINIM_FALSE = 0x06,
    MINIM_TRUE = 0x0E,
    MINIM_TYPE_CHAR = 0x16,
    MINIM_UNBOUND = 0x1E,
    MINIM_NIL = 0x26,
    MINIM_VOID = 0x2E,
    MINIM_EOF = 0x36,
    MINIM_FORWARD = 0x3E,  // GC forwarding marker, never a user value

    MINIM_CHAR_SHIFT = 8,
};

// Typed-object headers: (length << 4) | (immutable << 3) | type
enum {
    MINIM_HEADER_TYPE_MASK = 0x7,
    MINIM_HEADER_IMMUTABLE = 0x8,
    MINIM_HEADER_LENGTH_SHIFT = 4,

    MINIM_TYPE_VECTOR = 0x0,
    MINIM_TYPE_STRING = 0x2,
};

// Object sizes in bytes, before rounding to the allocation unit
enum {
    MINIM_PAIR_SIZE = 2 * sizeof(minim_obj),
    MINIM_SYMBOL_SIZE = 2 * sizeof(minim_obj),
};

static inline size_t minim_vector_size(size_t n) {
    return (1 + n) * sizeof(minim_obj);
}

static inline size_t minim_string_size(size_t n) {
    return sizeof(minim_obj) + n * sizeof(uint32_t);
}

// Fixnums: 61-bit signed integers stored as n << 3

#define MINIM_FIXNUM_MAX ((intptr_t)(((uintptr_t)1 << 60) - 1))
#define MINIM_FIXNUM_MIN (-MINIM_FIXNUM_MAX - 1)

static inline minim_obj minim_tag(minim_obj x) {
    return x & MINIM_TAG_MASK;
}

static inline minim_obj minim_fixnum(intptr_t n) {
    assert(n >= MINIM_FIXNUM_MIN && n <= MINIM_FIXNUM_MAX);
    return (minim_obj)n << MINIM_TAG_BITS;
}

static inline intptr_t minim_fixnum_value(minim_obj x) {
    return (intptr_t)x >> MINIM_TAG_BITS;
}

// Chars: Unicode scalar values stored as (c << 8) | MINIM_TYPE_CHAR

static inline bool minim_is_scalar_value(uint32_t c) {
    return c <= 0x10FFFF && (c < 0xD800 || c > 0xDFFF);
}

static inline minim_obj minim_char(uint32_t c) {
    assert(minim_is_scalar_value(c));
    return ((minim_obj)c << MINIM_CHAR_SHIFT) | MINIM_TYPE_CHAR;
}

static inline uint32_t minim_char_value(minim_obj x) {
    return (uint32_t)(x >> MINIM_CHAR_SHIFT);
}

// Typed objects

static inline minim_obj minim_make_header(size_t length, minim_obj type) {
    return ((minim_obj)length << MINIM_HEADER_LENGTH_SHIFT) | type;
}

static inline minim_obj minim_typed_header(minim_obj x) {
    return *(const minim_obj *)(x - MINIM_TAG_TYPED);
}

// Predicates

static inline bool minim_is_fixnum(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_FIXNUM;
}

static inline bool minim_is_pair(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_PAIR;
}

static inline bool minim_is_symbol(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_SYMBOL;
}

static inline bool minim_is_typed(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_TYPED;
}

static inline bool minim_is_vector(minim_obj x) {
    return minim_is_typed(x)
        && (minim_typed_header(x) & MINIM_HEADER_TYPE_MASK) == MINIM_TYPE_VECTOR;
}

static inline bool minim_is_string(minim_obj x) {
    return minim_is_typed(x)
        && (minim_typed_header(x) & MINIM_HEADER_TYPE_MASK) == MINIM_TYPE_STRING;
}

static inline bool minim_is_char(minim_obj x) {
    return (x & MINIM_IMMEDIATE_MASK) == MINIM_TYPE_CHAR;
}

static inline bool minim_is_boolean(minim_obj x) {
    return x == MINIM_FALSE || x == MINIM_TRUE;
}

static inline bool minim_is_null(minim_obj x) {
    return x == MINIM_NIL;
}

// Runtime

void minim_init(void);
void minim_shutdown(void);

// Garbage collection
//
// Any allocation may collect, which moves objects.  A local holding an object
// across a call that may allocate must be protected in a frame:
//
//     minim_obj list2(minim_obj a, minim_obj b) {
//         MINIM_FRAME(&a, &b);
//         minim_obj tail = minim_cons(b, MINIM_NIL);
//         MINIM_PROTECT(&tail);
//         MINIM_RETURN(minim_cons(a, tail));
//     }
//
// Leave a frame only through MINIM_RETURN or MINIM_RETURN_VOID.  Don't nest
// allocating calls (`minim_cons(a, minim_cons(b, c))`): `a` may be read before
// the inner call collects.

size_t minim_gc_mark(void);
void minim_gc_reset(size_t mark);
void minim_gc_protect(minim_obj *const *vars, size_t n);

// Opens a frame and protects the variables at the given addresses.
#define MINIM_FRAME(...)                       \
    size_t minim_frame_ = minim_gc_mark();     \
    MINIM_PROTECT(__VA_ARGS__)

// Protects the variables at the given addresses until the frame closes.
#define MINIM_PROTECT(...)                                       \
    minim_gc_protect((minim_obj *[]){__VA_ARGS__},               \
                     sizeof((minim_obj *[]){__VA_ARGS__}) / sizeof(minim_obj *))

// Closes the frame and returns `e`, which is evaluated inside the frame.
#define MINIM_RETURN(e)                        \
    do {                                       \
        minim_obj minim_result_ = (e);         \
        minim_gc_reset(minim_frame_);          \
        return minim_result_;                  \
    } while (0)

#define MINIM_RETURN_VOID                      \
    do {                                       \
        minim_gc_reset(minim_frame_);          \
        return;                                \
    } while (0)

// Registers a variable that holds an object for the life of the runtime.
void minim_gc_add_global(minim_obj *p);

void minim_gc_collect(void);

// Pairs

minim_obj minim_cons(minim_obj car, minim_obj cdr);

static inline minim_obj minim_car(minim_obj p) {
    assert(minim_is_pair(p));
    return ((const minim_obj *)(p - MINIM_TAG_PAIR))[0];
}

static inline minim_obj minim_cdr(minim_obj p) {
    assert(minim_is_pair(p));
    return ((const minim_obj *)(p - MINIM_TAG_PAIR))[1];
}

static inline void minim_set_car(minim_obj p, minim_obj x) {
    assert(minim_is_pair(p));
    ((minim_obj *)(p - MINIM_TAG_PAIR))[0] = x;
}

static inline void minim_set_cdr(minim_obj p, minim_obj x) {
    assert(minim_is_pair(p));
    ((minim_obj *)(p - MINIM_TAG_PAIR))[1] = x;
}

// Vectors

minim_obj minim_make_vector(size_t n, minim_obj fill);

static inline size_t minim_vector_length(minim_obj v) {
    assert(minim_is_vector(v));
    return (size_t)(minim_typed_header(v) >> MINIM_HEADER_LENGTH_SHIFT);
}

static inline minim_obj minim_vector_ref(minim_obj v, size_t i) {
    assert(i < minim_vector_length(v));
    return ((const minim_obj *)(v - MINIM_TAG_TYPED))[1 + i];
}

static inline void minim_vector_set(minim_obj v, size_t i, minim_obj x) {
    assert(i < minim_vector_length(v));
    ((minim_obj *)(v - MINIM_TAG_TYPED))[1 + i] = x;
}

// Strings: arrays of Unicode scalar values

minim_obj minim_make_string(size_t n, uint32_t c);

// Decodes UTF-8; invalid bytes become U+FFFD.
minim_obj minim_string_from_cstr(const char *s);

static inline size_t minim_string_length(minim_obj s) {
    assert(minim_is_string(s));
    return (size_t)(minim_typed_header(s) >> MINIM_HEADER_LENGTH_SHIFT);
}

// Valid until the next allocation.
static inline uint32_t *minim_string_chars(minim_obj s) {
    assert(minim_is_string(s));
    return (uint32_t *)(s - MINIM_TAG_TYPED + sizeof(minim_obj));
}

static inline uint32_t minim_string_ref(minim_obj s, size_t i) {
    assert(i < minim_string_length(s));
    return minim_string_chars(s)[i];
}

static inline void minim_string_set(minim_obj s, size_t i, uint32_t c) {
    assert(i < minim_string_length(s) && minim_is_scalar_value(c));
    assert(!(minim_typed_header(s) & MINIM_HEADER_IMMUTABLE));
    minim_string_chars(s)[i] = c;
}

// Symbols: interned, never freed.  New symbols are MINIM_UNBOUND.

// Returns the symbol named `str`, creating it with an immutable copy of
// `str` if needed.
minim_obj minim_intern(minim_obj str);
minim_obj minim_intern_cstr(const char *s);

static inline minim_obj minim_symbol_name(minim_obj sym) {
    assert(minim_is_symbol(sym));
    return ((const minim_obj *)(sym - MINIM_TAG_SYMBOL))[0];
}

static inline minim_obj minim_symbol_value(minim_obj sym) {
    assert(minim_is_symbol(sym));
    return ((const minim_obj *)(sym - MINIM_TAG_SYMBOL))[1];
}

static inline void minim_set_symbol_value(minim_obj sym, minim_obj x) {
    assert(minim_is_symbol(sym));
    ((minim_obj *)(sym - MINIM_TAG_SYMBOL))[1] = x;
}

#endif

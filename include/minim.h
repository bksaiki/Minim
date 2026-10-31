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
};

static inline size_t minim_vector_size(size_t n) {
    return (1 + n) * sizeof(minim_obj);
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

static inline minim_obj minim_char(uint32_t c) {
    assert(c <= 0x10FFFF && (c < 0xD800 || c > 0xDFFF));
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

#endif

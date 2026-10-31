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
#include <string.h>

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
    MINIM_TAG_CLOSURE = 0x5,
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
    MINIM_TYPE_PRIMITIVE = 0x4,
};

/** Allocation constants. */
enum {
    MINIM_ALIGN = 16,              ///< object alignment and size unit
    MINIM_CHUNK_SIZE = 16 * 1024,  ///< heap chunk size and alignment in bytes
};

/** Object sizes in bytes, before rounding to the allocation unit. */
enum {
    MINIM_PAIR_SIZE = 2 * sizeof(minim_obj),
    MINIM_SYMBOL_SIZE = 2 * sizeof(minim_obj),
    MINIM_CLOSURE_SIZE = 2 * sizeof(minim_obj),
    MINIM_PRIMITIVE_SIZE = 5 * sizeof(minim_obj),
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

/** Whether `x` is a closure. */
static inline bool minim_is_closure(minim_obj x) {
    return minim_tag(x) == MINIM_TAG_CLOSURE;
}

/** Whether `x` is a primitive. */
static inline bool minim_is_primitive(minim_obj x) {
    return minim_is_typed(x)
        && minim_header_type(minim_typed_header(x)) == MINIM_TYPE_PRIMITIVE;
}

/** Whether `x` is a closure or a primitive. */
static inline bool minim_is_procedure(minim_obj x) {
    return minim_is_closure(x) || minim_is_primitive(x);
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

/**
 * Collector settings.  The n-th collection collects generations 0 through the
 * largest g at most `max_generation` such that `radix`^g divides n.  Every
 * setting is at least 1 and at most MINIM_FIXNUM_MAX; `max_generation` is at
 * most 8.
 */
typedef struct minim_gc_config {
    size_t nursery_bytes;           ///< bytes allocated between collections
    size_t radix;                   ///< base of the generation schedule
    size_t max_generation;          ///< the oldest generation
    size_t stack_initial_capacity;  ///< elements the collector's stacks first allocate
} minim_gc_config;

/**
 * The default collector settings: a 1 MiB nursery, radix 4, and four old
 * generations, so generation g is collected every 4^g collections.
 */
minim_gc_config minim_gc_default_config(void);

/** The collector settings. */
minim_gc_config minim_gc_get_config(void);

/**
 * Sets the collector settings.  The nursery and the schedule start over;
 * `stack_initial_capacity` affects only stacks not yet allocated, since stacks
 * never shrink.
 */
void minim_gc_set_config(minim_gc_config config);

/** Runs the collection the schedule calls for. */
void minim_gc_collect(void);

/** Collects generation 0, the nursery, promoting its survivors to generation 1. */
void minim_gc_collect_minor(void);

/** Collects every generation, copying every live object. */
void minim_gc_collect_major(void);

/**
 * Collects generations 0 through `g`, promoting their survivors to `g + 1`.
 * Collecting the oldest collects every generation into it.
 */
void minim_gc_collect_generation(unsigned g);

/** Remembers the old object `obj` until the next collection. */
void minim_gc_remember(minim_obj obj);

/** Write barrier, after storing `x` into `obj`: remembers `obj` if old and `x` is a heap object. */
static inline void minim_gc_write(minim_obj obj, minim_obj x) {
    minim_obj tag = minim_tag(x);
    // An object's chunk, found by alignment, starts with its generation.
    if (tag != MINIM_TAG_FIXNUM && tag != MINIM_TAG_IMMEDIATE
        && *(const unsigned char *)(obj & ~(minim_obj)(MINIM_CHUNK_SIZE - 1)) != 0)
        minim_gc_remember(obj);
}

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
    minim_gc_write(p, x);
}

/** Sets the second field of the pair `p`. */
static inline void minim_set_cdr(minim_obj p, minim_obj x) {
    assert(minim_is_pair(p));
    ((minim_obj *)(p - MINIM_TAG_PAIR))[1] = x;
    minim_gc_write(p, x);
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
    minim_gc_write(v, x);
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

/** Whether the strings `a` and `b` have the same chars. */
static inline bool minim_string_equal(minim_obj a, minim_obj b) {
    size_t n = minim_string_length(a);
    return n == minim_string_length(b)
        && memcmp(minim_string_chars(a), minim_string_chars(b), n * sizeof(uint32_t)) == 0;
}

/** Initial value of a hash (the FNV-1a offset basis). */
#define MINIM_HASH_INIT UINT64_C(14695981039346656037)

/** Mixes `x` into the hash `h` (one FNV-1a step). */
static inline uint64_t minim_hash_mix(uint64_t h, uint64_t x) {
    return (h ^ x) * UINT64_C(1099511628211);
}

/** Hash of the chars of the string `s`. */
static inline uint64_t minim_string_hash(minim_obj s) {
    const uint32_t *chars = minim_string_chars(s);
    uint64_t h = MINIM_HASH_INIT;
    for (size_t i = 0; i < minim_string_length(s); i++)
        h = minim_hash_mix(h, chars[i]);
    return h;
}

/** Sets char `i` of the mutable string `s`. */
static inline void minim_string_set(minim_obj s, size_t i, uint32_t c) {
    assert(i < minim_string_length(s) && minim_is_scalar_value(c));
    assert(!(minim_typed_header(s) & MINIM_HEADER_IMMUTABLE));
    minim_string_chars(s)[i] = c;
}

// Symbols: interned ones are never freed

/** Interned symbol named `str`; a new one is made as by minim_make_uninterned. */
minim_obj minim_intern(minim_obj str);

/** New unbound, uninterned symbol named `str`, which is copied unless immutable. */
minim_obj minim_make_uninterned(minim_obj str);

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
    minim_gc_write(sym, x);
}

// Procedures

/** New closure over `code`, a `(params . body)` pair, in `env`. */
minim_obj minim_make_closure(minim_obj code, minim_obj env);

/** Code, `(params . body)`, of the closure `c`. */
static inline minim_obj minim_closure_code(minim_obj c) {
    assert(minim_is_closure(c));
    return ((const minim_obj *)(c - MINIM_TAG_CLOSURE))[0];
}

/** Environment of the closure `c`. */
static inline minim_obj minim_closure_env(minim_obj c) {
    assert(minim_is_closure(c));
    return ((const minim_obj *)(c - MINIM_TAG_CLOSURE))[1];
}

/** C function of a primitive, called with its `n` arguments. */
typedef minim_obj (*minim_prim_fn)(minim_obj *args, size_t n);

/** Maximum arity of a primitive that takes any number of arguments. */
enum { MINIM_VARIADIC = -1 };

/**
 * New primitive named by the symbol `name`, taking between `min_arity` and
 * `max_arity` arguments (or MINIM_VARIADIC).
 */
minim_obj minim_make_primitive(minim_obj name, minim_prim_fn fn,
                               intptr_t min_arity, intptr_t max_arity);

/** Function of the primitive `p`. */
static inline minim_prim_fn minim_primitive_fn(minim_obj p) {
    assert(minim_is_primitive(p));
    return (minim_prim_fn)((const minim_obj *)(p - MINIM_TAG_TYPED))[1];
}

/** Name of the primitive `p`. */
static inline minim_obj minim_primitive_name(minim_obj p) {
    assert(minim_is_primitive(p));
    return ((const minim_obj *)(p - MINIM_TAG_TYPED))[2];
}

/** Minimum arity of the primitive `p`. */
static inline intptr_t minim_primitive_min_arity(minim_obj p) {
    assert(minim_is_primitive(p));
    return minim_fixnum_value(((const minim_obj *)(p - MINIM_TAG_TYPED))[3]);
}

/** Maximum arity of the primitive `p`, or MINIM_VARIADIC. */
static inline intptr_t minim_primitive_max_arity(minim_obj p) {
    assert(minim_is_primitive(p));
    return minim_fixnum_value(((const minim_obj *)(p - MINIM_TAG_TYPED))[4]);
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
 * returns `false` if it raised an error, restoring the root and value stacks.
 */
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);

/** Message and irritant of the last error. */
void minim_last_error(const char **msg, minim_obj *irritant);

/** Writes the last error to `out` as `error: msg: irritant`. */
void minim_write_error(FILE *out);

// Evaluation

/** Value of the top-level form `expr`, expanded by `%expand` if bound.  Raises on errors. */
minim_obj minim_eval(minim_obj expr);

// Reading

/** Reads the next datum from `in`, or returns MINIM_EOF.  Raises on bad syntax. */
minim_obj minim_read(FILE *in);

// Printing

/** Writes `x` to `out` in `write` syntax.  Never allocates. */
void minim_write(FILE *out, minim_obj x);

/** Like minim_write, but strings and chars print as their characters. */
void minim_display(FILE *out, minim_obj x);

#endif

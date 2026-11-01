/**
 * @file
 * Heap: chunked spaces with bump allocation.
 */

#ifndef MINIM_HEAP_H
#define MINIM_HEAP_H

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "minim.h"

/** Defined when built with AddressSanitizer. */
#if defined(__SANITIZE_ADDRESS__)
#define MINIM_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MINIM_ASAN 1
#endif
#endif

#ifdef MINIM_ASAN
/** Spare chunks kept before one is reused, so a stale pointer stays poisoned. */
#define MINIM_CHUNK_QUARANTINE 8
#else
#define MINIM_CHUNK_QUARANTINE 0
#endif

/** `n` rounded up to a multiple of MINIM_ALIGN. */
static inline size_t minim_align_up(size_t n) {
    return (n + MINIM_ALIGN - 1) & ~(size_t)(MINIM_ALIGN - 1);
}

/**
 * A block of heap memory, aligned to MINIM_CHUNK_SIZE.  A chunk is that size,
 * or a multiple of it for an object too large for one; either way, every
 * object starts in its chunk's first MINIM_CHUNK_SIZE bytes.
 */
typedef struct minim_chunk {
    unsigned char generation;  ///< its objects' generation
    struct minim_chunk *next;  ///< next newer chunk in its space, or next spare
    unsigned char *end;        ///< end of used data, once a newer chunk exists
    size_t size;               ///< capacity of `data`
    /**
     * A byte per MINIM_ALIGN bytes: for a remembered object starting there, 1 + the
     * youngest generation it may point to; otherwise 0.
     */
    unsigned char remembered[MINIM_CHUNK_SIZE / MINIM_ALIGN];
    alignas(MINIM_ALIGN) unsigned char data[];
} minim_chunk;

static_assert(offsetof(minim_chunk, generation) == 0, "minim_gc_write reads the first byte");

/** The chunk holding the object at address `p`; `p` may be tagged. */
static inline minim_chunk *minim_chunk_of(uintptr_t p) {
    return (minim_chunk *)(p & ~(uintptr_t)(MINIM_CHUNK_SIZE - 1));
}

/** Chunks in a first-in, first-out queue, linked by `next`. */
typedef struct minim_chunk_queue {
    minim_chunk *first;
    minim_chunk *last;
    size_t len;
} minim_chunk_queue;

/** Chunks in allocation order, with a bump pointer into the last one. */
typedef struct minim_space {
    minim_chunk *first;
    minim_chunk *last;
    unsigned char *alloc;      ///< next free byte in `last`
    size_t avail;              ///< free bytes in `last`
    unsigned char generation;  ///< of every chunk in the space
} minim_space;

/** End of the objects in `c`, a chunk of `s`. */
static inline unsigned char *minim_chunk_end(const minim_space *s, const minim_chunk *c) {
    return c == s->last ? s->alloc : c->end;
}

/** Kinds of object, each with its own space in every old generation. */
enum {
    MINIM_KIND_PAIR,  ///< pairs and closures
    MINIM_KIND_SYMBOL,
    MINIM_KIND_TYPED,
    MINIM_KIND_COUNT,
};

/** The most old generations the heap holds. */
enum { MINIM_MAX_GENERATION = 8 };

/** Reports that memory is exhausted and aborts. */
_Noreturn void minim_out_of_memory(void);

/** Allocates `nbytes` in the nursery; may collect first. */
void *minim_heap_alloc(size_t nbytes);

/** Allocates `nbytes` in `s`; never collects. */
void *minim_space_alloc(minim_space *s, size_t nbytes);

/**
 * Empties `s`, keeping its generation.  Its standard-size chunks become spares;
 * large ones are freed.
 */
void minim_space_free(minim_space *s);

/**
 * Spare-chunk limit: the chunks in use (room for a major collection), plus the
 * nursery's worth and the quarantine.
 */
size_t minim_heap_max_spares(void);

/** Frees the oldest spare chunks beyond minim_heap_max_spares(). */
void minim_heap_trim_spares(void);

/** Frees the spare chunks. */
void minim_heap_free_spares(void);

#endif

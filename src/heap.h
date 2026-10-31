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
    unsigned char generation;  ///< 0 for new objects, 1 for survivors; first, for minim_gc_write
    struct minim_chunk *next;  ///< next newer chunk
    unsigned char *end;        ///< end of used data, once a newer chunk exists
    size_t size;               ///< capacity of `data`
    /// A bit per MINIM_ALIGN bytes: whether the object starting there is remembered.
    uint64_t remembered[MINIM_CHUNK_SIZE / MINIM_ALIGN / 64];
    alignas(MINIM_ALIGN) unsigned char data[];
} minim_chunk;

static_assert(offsetof(minim_chunk, generation) == 0, "minim_gc_write reads the first byte");

/** The chunk holding the object at address `p`; `p` may be tagged. */
static inline minim_chunk *minim_chunk_of(uintptr_t p) {
    return (minim_chunk *)(p & ~(uintptr_t)(MINIM_CHUNK_SIZE - 1));
}

/** Chunks in allocation order, with a bump pointer into the last one. */
typedef struct minim_space {
    minim_chunk *first;
    minim_chunk *last;
    unsigned char *alloc;      ///< next free byte in `last`
    size_t avail;              ///< free bytes in `last`
    unsigned char generation;  ///< of every chunk in the space
} minim_space;

/**
 * Heap spaces.  New objects, the young generation, go in MINIM_SPACE_NEW;
 * survivors are copied into the old generation's space for their kind.
 */
enum {
    MINIM_SPACE_NEW,
    MINIM_SPACE_PAIR,
    MINIM_SPACE_SYMBOL,
    MINIM_SPACE_TYPED,
    MINIM_SPACE_COUNT,
};

/** Reports that memory is exhausted and aborts. */
_Noreturn void minim_out_of_memory(void);

/** Allocates `nbytes` in the new area; may collect first. */
void *minim_heap_alloc(size_t nbytes);

/** Allocates `nbytes` in `s`; never collects. */
void *minim_space_alloc(minim_space *s, size_t nbytes);

/** Frees every chunk in `s` and empties it, keeping its generation. */
void minim_space_free(minim_space *s);

/** Bytes in use in `s`. */
size_t minim_space_used(const minim_space *s);

#endif

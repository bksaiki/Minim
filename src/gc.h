/**
 * @file
 * Collector state and lifecycle.  The collector API is in minim.h.
 */

#ifndef MINIM_GC_H
#define MINIM_GC_H

#include <stddef.h>

#include "heap.h"
#include "minim.h"

/** A growable array of objects. */
typedef struct minim_obj_stack {
    minim_obj *items;
    size_t len;
    size_t cap;
} minim_obj_stack;

/** The value stack (`stack.h`): a fixed buffer over a chain of frozen segments. */
typedef struct minim_value_stack {
    minim_obj *items;  ///< MINIM_SEGMENT_SIZE values, never reallocated
    size_t len;        ///< values in the buffer
    size_t start;      ///< height of items[0]
    minim_obj below;   ///< the newest frozen segment, or '()
} minim_value_stack;

/** A growable array of variables holding objects. */
typedef struct minim_root_stack {
    minim_obj **items;
    size_t len;
    size_t cap;
} minim_root_stack;

/** All collector state; minim_gc_init and minim_gc_shutdown reset it. */
extern struct minim_gc {
    minim_gc_config config;
    minim_space nursery;  ///< generation 0, every kind interleaved
    /** Generation g's space for each kind, for g from 1; row 0 is unused. */
    minim_space old[MINIM_MAX_GENERATION + 1][MINIM_KIND_COUNT];
    size_t old_bytes[MINIM_MAX_GENERATION + 1];  ///< bytes in each old generation
    minim_chunk_queue spares;    ///< freed standard-size chunks, reused oldest first
    minim_root_stack locals;     ///< the shadow stack
    minim_root_stack globals;    ///< from minim_gc_add_global
    minim_value_stack values;    ///< values of calls in progress, updated in place
    minim_obj_stack spill;       ///< arguments `apply` gives a primitive, too many for `values`
    minim_obj_stack remembered;  ///< old objects that may point younger, each once
    size_t size;                 ///< bytes held in chunks
    size_t allocated;            ///< bytes handed out by minim_heap_alloc since init
    size_t live;                 ///< bytes in the old generations after the last collection
    size_t budget;               ///< bytes to allocate before the next collection
    size_t scheduled;            ///< calls to minim_gc_collect
    /** Collections, by the oldest generation each collected. */
    size_t collections[MINIM_MAX_GENERATION + 1];
    unsigned collecting;         ///< in a collection, the oldest generation it collects
    unsigned target;             ///< in a collection, the generation survivors go to
} minim_gc;

/** Resets the collector, with its settings from the environment. */
void minim_gc_init(void);

/** Frees the heap and the stacks. */
void minim_gc_shutdown(void);

#endif

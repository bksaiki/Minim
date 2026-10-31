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

/** A growable array of variables holding objects. */
typedef struct minim_root_stack {
    minim_obj **items;
    size_t len;
    size_t cap;
} minim_root_stack;

/** All collector state; minim_gc_init and minim_gc_shutdown reset it. */
extern struct minim_gc {
    minim_gc_config config;
    minim_gc_config program_config;  ///< from the environment, for after startup
    minim_space nursery;  ///< generation 0, every kind interleaved
    /** Generation g's space for each kind, for g from 1; row 0 is unused. */
    minim_space old[MINIM_MAX_GENERATION + 1][MINIM_KIND_COUNT];
    size_t old_bytes[MINIM_MAX_GENERATION + 1];  ///< bytes in each old generation
    minim_chunk_queue spares;    ///< freed standard-size chunks, reused oldest first
    minim_root_stack locals;     ///< the shadow stack
    minim_root_stack globals;    ///< from minim_gc_add_global
    minim_obj_stack values;      ///< values of calls in progress, updated in place
    minim_obj_stack remembered;  ///< old objects written since the last collection, each once
    size_t size;                 ///< bytes held in chunks
    size_t allocated;            ///< bytes handed out by minim_heap_alloc since init
    size_t live;                 ///< bytes in the old generations after the last collection
    size_t budget;               ///< bytes to allocate before the next collection
    size_t major_base;           ///< bytes in the old generation after the last major collection
    size_t scheduled;            ///< calls to minim_gc_collect
    size_t minor_collections;
    size_t major_collections;
    unsigned collecting;         ///< in a collection, the oldest generation it collects
    unsigned target;             ///< in a collection, the generation survivors go to
} minim_gc;

/** Pushes `x` onto the value stack; may move `minim_gc.values.items`. */
void minim_values_push(minim_obj x);

/**
 * Resets the collector and reads its settings from the environment.  They
 * apply once startup finishes, or from now if MINIM_GC_STARTUP is 1; until
 * then, the defaults apply.
 */
void minim_gc_init(void);

/** Applies the environment's settings; call last in minim_init. */
void minim_gc_finish_startup(void);

/** Frees the heap and the stacks. */
void minim_gc_shutdown(void);

#endif

/**
 * @file
 * Collector lifecycle.  The collector API is in minim.h.
 */

#ifndef MINIM_GC_H
#define MINIM_GC_H

#include <stddef.h>

#include "minim.h"

/**
 * Values of calls in progress, scanned as roots in `[0, len)`.  The collector
 * updates them in place.
 */
extern struct minim_values {
    minim_obj *items;
    size_t len;
    size_t cap;
} minim_values;

/** Pushes `x`; may move `minim_values.items`. */
void minim_values_push(minim_obj x);

/** Initializes the collector; call after minim_heap_init. */
void minim_gc_init(void);

/** Frees the root and value stacks. */
void minim_gc_shutdown(void);

#endif

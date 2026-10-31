/**
 * @file
 * Collector lifecycle.  The collector API is in minim.h.
 */

#ifndef MINIM_GC_H
#define MINIM_GC_H

/** Initializes the collector; call after minim_heap_init. */
void minim_gc_init(void);

/** Frees the root stacks. */
void minim_gc_shutdown(void);

#endif

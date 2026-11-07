/**
 * @file
 * Hash tables from heap objects, by address, to words.
 */

#ifndef MINIM_TABLE_H
#define MINIM_TABLE_H

#include <stdint.h>

#include "minim.h"

/**
 * A hash table from heap objects to words, in malloc'd memory.  Keyed by
 * address, so valid only while nothing allocates on the heap.  Zero it to
 * start empty.
 */
typedef struct minim_table {
    minim_obj *keys;    ///< 0 for an empty slot
    uintptr_t *values;
    size_t len;
    size_t cap;         ///< a power of two, or 0
} minim_table;

/** The slot of the value of `key` in `t`, or NULL if it isn't there. */
uintptr_t *minim_table_find(const minim_table *t, minim_obj key);

/**
 * The slot of the value of `key` in `t`, adding `key` with value 0 if it isn't
 * there.  Adding may move every slot.
 */
uintptr_t *minim_table_add(minim_table *t, minim_obj key);

/** Frees the memory of `t`, leaving it empty. */
void minim_table_free(minim_table *t);

#endif

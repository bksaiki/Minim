/**
 * @file
 * Hash tables from heap objects, by address: to words, and to small marks.
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

/**
 * A set of heap objects, each with a mark from 1 to 7, in malloc'd memory:
 * one word per object, its address with the mark in the low bits, which
 * alignment leaves clear.  Keyed by address, so valid only while nothing
 * allocates on the heap.  Zero it to start empty.
 */
typedef struct minim_marks {
    uintptr_t *slots;  ///< 0 for an empty slot
    size_t len;
    size_t cap;        ///< a power of two, or 0
} minim_marks;

/** The mark of `x` in `m`; if 0, `x` wasn't there, and is added with `mark`. */
unsigned minim_marks_visit(minim_marks *m, minim_obj x, unsigned mark);

/** Sets the mark of `x`, which is in `m`. */
void minim_marks_set(minim_marks *m, minim_obj x, unsigned mark);

/** Frees the memory of `m`, leaving it empty. */
void minim_marks_free(minim_marks *m);

#endif

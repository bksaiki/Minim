/**
 * @file
 * The value stack: the operators and operands of calls in progress, and the
 * values being returned.  Its top is a fixed C buffer; below it, a chain of
 * frozen segments, heap vectors `#(prev start v ...)`, holds the rest.  A
 * value keeps its height when frozen, so heights are logical.
 */

#ifndef MINIM_STACK_H
#define MINIM_STACK_H

#include <assert.h>
#include <stddef.h>

#include "gc.h"

/** Values the buffer holds; a call takes at most one fewer arguments. */
enum { MINIM_SEGMENT_SIZE = 8192, MINIM_MAX_ARGS = MINIM_SEGMENT_SIZE - 1 };

/** Height of the top of the stack. */
static inline size_t minim_stack_height(void) {
    return minim_gc.values.start + minim_gc.values.len;
}

/** The slot at height `h`, which must be in the buffer (or just above its top). */
static inline minim_obj *minim_stack_at(size_t h) {
    assert(h >= minim_gc.values.start);
    return minim_gc.values.items + (h - minim_gc.values.start);
}

/** Pops to height `h`, which must be in the buffer. */
static inline void minim_stack_pop_to(size_t h) {
    assert(h >= minim_gc.values.start && h <= minim_stack_height());
    minim_gc.values.len = h - minim_gc.values.start;
}

/** Pushes `x` onto a full buffer, freezing it first. */
void minim_stack_push_full(minim_obj x);

/** Pushes `x`, freezing the buffer first if it is full. */
static inline void minim_stack_push(minim_obj x) {
    if (minim_gc.values.len == MINIM_SEGMENT_SIZE)
        minim_stack_push_full(x);
    else
        minim_gc.values.items[minim_gc.values.len++] = x;
}

/** As minim_stack_reserve, when the buffer lacks room for `n` values. */
void minim_stack_reserve_more(size_t n);

/**
 * Makes room for a call of `n` values (its operator and operands) at the top,
 * freezing the buffer if they don't fit; raises `too many arguments` if they
 * can't.
 */
static inline void minim_stack_reserve(size_t n) {
    if (MINIM_SEGMENT_SIZE - minim_gc.values.len < n)
        minim_stack_reserve_more(n);
}

/** Freezes the buffer's values into a new segment, emptying it. */
void minim_stack_freeze(void);

/** Freezes the values below height `h`, so the buffer starts at `h`. */
void minim_stack_relocate(size_t h);

/** As minim_stack_reload, when `h` is below the buffer. */
void minim_stack_reload_below(size_t h);

/** Brings height `h` into the empty buffer by reloading the frozen values below it. */
static inline void minim_stack_reload(size_t h) {
    if (h < minim_gc.values.start)
        minim_stack_reload_below(h);
}

/** Pops to height `h`, which may be below the buffer, reloading frozen values. */
void minim_stack_unwind(size_t h);

/** Brings the frozen value at height `h`, just below the buffer, into it. */
void minim_stack_include(size_t h);

/** The frozen segments holding the values below height `h`, which is in the buffer. */
minim_obj minim_stack_capture(size_t h);

/**
 * Replaces the stack with the frozen segments `stack`, ending at `height`,
 * with the `n` values at height `from` on top.
 */
void minim_stack_reinstate(minim_obj stack, size_t height, size_t from, size_t n);

/**
 * Copies the values from height `h` to the top, then the elements of the list
 * `lst`, into `minim_gc.spill`; returns how many.
 */
size_t minim_stack_spill(size_t h, minim_obj lst);

#endif

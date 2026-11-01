/**
 * @file
 * The value stack's buffer and frozen segments.
 */

#include "stack.h"

#include <stdlib.h>
#include <string.h>

// Slots of a frozen segment: the next older segment, the height of its first
// value, then the values.
enum { SEG_PREV, SEG_START, SEG_VALUES };

static size_t seg_start(minim_obj seg) {
    return (size_t)minim_fixnum_value(minim_vector_ref(seg, SEG_START));
}

// Freezes the buffer's first `k` values into a new segment, moving the rest
// down.
static void freeze(size_t k) {
    minim_obj seg = minim_make_vector(k + SEG_VALUES, MINIM_NIL);  // the whole buffer is a root
    minim_value_stack *s = &minim_gc.values;
    minim_vector_set(seg, SEG_PREV, s->below);
    minim_vector_set(seg, SEG_START, minim_fixnum((intptr_t)s->start));
    for (size_t i = 0; i < k; i++)
        minim_vector_set(seg, SEG_VALUES + i, s->items[i]);
    memmove(s->items, s->items + k, (s->len - k) * sizeof(minim_obj));
    s->len -= k;
    s->start += k;
    s->below = seg;
}

void minim_stack_freeze(void) {
    if (minim_gc.values.len > 0)
        freeze(minim_gc.values.len);
}

void minim_stack_relocate(size_t h) {
    assert(h >= minim_gc.values.start && h <= minim_stack_height());
    if (h > minim_gc.values.start)
        freeze(h - minim_gc.values.start);
}

void minim_stack_push_full(minim_obj x) {
    MINIM_FRAME(&x);
    minim_stack_freeze();
    minim_gc.values.items[minim_gc.values.len++] = x;
    MINIM_RETURN_VOID;
}

void minim_stack_reserve_more(size_t n) {
    if (n > MINIM_SEGMENT_SIZE)
        minim_error("too many arguments", minim_fixnum((intptr_t)n - 1));
    minim_stack_freeze();
}

void minim_stack_reload_below(size_t h) {
    minim_value_stack *s = &minim_gc.values;
    assert(s->len == 0);
    minim_obj seg = s->below;
    size_t start = seg_start(seg);
    assert(start <= h);
    (void)h;
    // Only the values below the buffer: any a `minim_stack_include` copied up
    // are in use from the buffer.
    s->len = s->start - start;
    for (size_t i = 0; i < s->len; i++)
        s->items[i] = minim_vector_ref(seg, SEG_VALUES + i);
    s->start = start;
    s->below = minim_vector_ref(seg, SEG_PREV);
}

void minim_stack_unwind(size_t h) {
    minim_value_stack *s = &minim_gc.values;
    if (h < s->start) {
        s->len = 0;
        while (s->below != MINIM_NIL && seg_start(s->below) >= h) {  // wholly above `h`
            s->start = seg_start(s->below);
            s->below = minim_vector_ref(s->below, SEG_PREV);
        }
        minim_stack_reload(h);
    }
    minim_stack_pop_to(h);
}

void minim_stack_include(size_t h) {
    minim_value_stack *s = &minim_gc.values;
    assert(h + 1 == s->start && s->len < MINIM_SEGMENT_SIZE);
    minim_obj x = minim_vector_ref(s->below, SEG_VALUES + (h - seg_start(s->below)));
    memmove(s->items + 1, s->items, s->len * sizeof(minim_obj));
    s->items[0] = x;
    s->start = h;
    s->len++;
}

// Appends `x` to `minim_gc.spill`, growing it with the C heap.
static void spill_push(minim_obj x) {
    minim_obj_stack *s = &minim_gc.spill;
    if (s->len == s->cap) {
        s->cap = s->cap ? 2 * s->cap : MINIM_SEGMENT_SIZE;
        s->items = realloc(s->items, s->cap * sizeof(minim_obj));
        if (s->items == NULL)
            minim_out_of_memory();
    }
    s->items[s->len++] = x;
}

size_t minim_stack_spill(size_t h, minim_obj lst) {
    minim_gc.spill.len = 0;
    for (; h < minim_stack_height(); h++)
        spill_push(*minim_stack_at(h));
    for (; minim_is_pair(lst); lst = minim_cdr(lst))
        spill_push(minim_car(lst));
    return minim_gc.spill.len;
}

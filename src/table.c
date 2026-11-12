/**
 * @file
 * Hash tables from heap objects, by address: open addressing, probing
 * linearly, at most half full.
 */

#include "table.h"

#include <assert.h>
#include <stdlib.h>

#include "heap.h"

// The first slot to probe for the word `key` in a table of capacity `cap`.
static size_t home(uintptr_t key, size_t cap) {
    return (size_t)(((uint64_t)key * UINT64_C(0x9E3779B97F4A7C15)) >> 32) & (cap - 1);
}

// The slot of `key` in `t`, or the empty slot where it would go.
static size_t probe(const minim_table *t, minim_obj key) {
    size_t i = home((uintptr_t)key, t->cap);
    while (t->keys[i] != 0 && t->keys[i] != key)
        i = (i + 1) & (t->cap - 1);
    return i;
}

uintptr_t *minim_table_find(const minim_table *t, minim_obj key) {
    if (t->cap == 0)
        return NULL;
    size_t i = probe(t, key);
    return t->keys[i] == key ? &t->values[i] : NULL;
}

// Doubles the capacity of `t`, rehashing its entries.
static void grow(minim_table *t) {
    minim_table old = *t;
    t->cap = old.cap ? 2 * old.cap : 64;
    t->keys = calloc(t->cap, sizeof(*t->keys));
    t->values = malloc(t->cap * sizeof(*t->values));
    if (t->keys == NULL || t->values == NULL)
        minim_out_of_memory();
    for (size_t i = 0; i < old.cap; i++) {
        if (old.keys[i] != 0) {
            size_t j = probe(t, old.keys[i]);
            t->keys[j] = old.keys[i];
            t->values[j] = old.values[i];
        }
    }
    free(old.keys);
    free(old.values);
}

uintptr_t *minim_table_add(minim_table *t, minim_obj key) {
    if (2 * (t->len + 1) > t->cap)
        grow(t);
    size_t i = probe(t, key);
    if (t->keys[i] == 0) {
        t->keys[i] = key;
        t->values[i] = 0;
        t->len++;
    }
    return &t->values[i];
}

void minim_table_free(minim_table *t) {
    free(t->keys);
    free(t->values);
    *t = (minim_table){0};
}

static const uintptr_t MARK_MASK = MINIM_TAG_MASK;

// The slot of the object at `addr` in `m`, or the empty one where it would go.
static uintptr_t *marks_slot(const minim_marks *m, uintptr_t addr) {
    size_t i = home(addr, m->cap);
    while (m->slots[i] != 0 && (m->slots[i] & ~MARK_MASK) != addr)
        i = (i + 1) & (m->cap - 1);
    return &m->slots[i];
}

unsigned minim_marks_visit(minim_marks *m, minim_obj x, unsigned mark) {
    assert(mark >= 1 && mark <= MARK_MASK);
    if (2 * (m->len + 1) > m->cap) {
        minim_marks old = *m;
        m->cap = old.cap ? 2 * old.cap : 64;
        m->slots = calloc(m->cap, sizeof(*m->slots));
        if (m->slots == NULL)
            minim_out_of_memory();
        for (size_t i = 0; i < old.cap; i++) {
            if (old.slots[i] != 0)
                *marks_slot(m, old.slots[i] & ~MARK_MASK) = old.slots[i];
        }
        free(old.slots);
    }
    uintptr_t addr = x & ~MARK_MASK;
    uintptr_t *slot = marks_slot(m, addr);
    if (*slot != 0)
        return (unsigned)(*slot & MARK_MASK);
    *slot = addr | mark;
    m->len++;
    return 0;
}

void minim_marks_set(minim_marks *m, minim_obj x, unsigned mark) {
    assert(mark >= 1 && mark <= MARK_MASK);
    uintptr_t addr = x & ~MARK_MASK;
    *marks_slot(m, addr) = addr | mark;
}

void minim_marks_free(minim_marks *m) {
    free(m->slots);
    *m = (minim_marks){0};
}

void minim_walk_push(minim_walk *w, minim_obj x) {
    if ((!minim_is_pair(x) && !minim_is_vector(x)) || minim_marks_visit(&w->queued, x, 1) != 0)
        return;
    if (w->len == w->cap) {
        w->cap = w->cap ? 2 * w->cap : 64;
        w->items = realloc(w->items, w->cap * sizeof(*w->items));
        if (w->items == NULL)
            minim_out_of_memory();
    }
    w->items[w->len++] = x;
}

void minim_walk_free(minim_walk *w) {
    free(w->items);
    minim_marks_free(&w->queued);
    *w = (minim_walk){0};
}

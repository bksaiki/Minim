/**
 * @file
 * Hash tables from heap objects, by address, to words: open addressing,
 * probing linearly, at most half full.
 */

#include "table.h"

#include <stdlib.h>

#include "heap.h"

// The first slot to probe for `key` in a table of capacity `cap`.
static size_t home(minim_obj key, size_t cap) {
    return (size_t)(((uint64_t)key * UINT64_C(0x9E3779B97F4A7C15)) >> 32) & (cap - 1);
}

// The slot of `key` in `t`, or the empty slot where it would go.
static size_t probe(const minim_table *t, minim_obj key) {
    size_t i = home(key, t->cap);
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

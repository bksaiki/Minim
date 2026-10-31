/**
 * @file
 * Chunk and space allocation.
 */

#include "heap.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "minim.h"

struct minim_heap minim_heap;

void minim_heap_init(void) {
    minim_heap = (struct minim_heap){0};
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++)
        minim_heap.spaces[i].generation = i == MINIM_SPACE_NEW ? 0 : 1;
}

void minim_heap_shutdown(void) {
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++)
        minim_space_free(&minim_heap.spaces[i]);
    minim_heap = (struct minim_heap){0};
}

void minim_out_of_memory(void) {
    fprintf(stderr, "minim: out of memory\n");
    abort();
}

#ifdef _MSC_VER
static void *chunk_alloc(size_t size) {
    return _aligned_malloc(size, MINIM_CHUNK_SIZE);
}

static void chunk_free(void *p) {
    _aligned_free(p);
}
#else
static void *chunk_alloc(size_t size) {
    return aligned_alloc(MINIM_CHUNK_SIZE, size);
}

static void chunk_free(void *p) {
    free(p);
}
#endif

// Bytes allocated for a chunk with `capacity` bytes of data.
static size_t chunk_bytes(size_t capacity) {
    return offsetof(minim_chunk, data) + capacity;
}

// Closes the last chunk of `s` and appends one that fits `nbytes`.
static void space_grow(minim_space *s, size_t nbytes) {
    // A multiple of MINIM_CHUNK_SIZE, as aligned_alloc requires.
    size_t bytes = (chunk_bytes(nbytes) + MINIM_CHUNK_SIZE - 1) & ~(size_t)(MINIM_CHUNK_SIZE - 1);
    minim_chunk *c = chunk_alloc(bytes);
    if (c == NULL)
        minim_out_of_memory();
    size_t size = bytes - offsetof(minim_chunk, data);
    *c = (minim_chunk){.generation = s->generation, .size = size};

    if (s->last != NULL) {
        s->last->end = s->alloc;
        s->last->next = c;
    } else {
        s->first = c;
    }
    s->last = c;
    s->alloc = c->data;
    // A large object's chunk holds nothing else, so every object starts in its
    // chunk's first MINIM_CHUNK_SIZE bytes.
    s->avail = bytes > MINIM_CHUNK_SIZE ? nbytes : size;
    minim_heap.size += bytes;
}

void *minim_space_alloc(minim_space *s, size_t nbytes) {
    nbytes = minim_align_up(nbytes);
    if (nbytes > s->avail)
        space_grow(s, nbytes);
    void *p = s->alloc;
    s->alloc += nbytes;
    s->avail -= nbytes;
    return p;
}

void *minim_heap_alloc(size_t nbytes) {
    nbytes = minim_align_up(nbytes);
#ifdef MINIM_GC_STRESS
    static unsigned long count;
    if (++count % MINIM_GC_STRESS == 0)
        minim_gc_collect_major();
#else
    if (nbytes > minim_heap.budget)
        minim_gc_collect();
#endif
    minim_heap.budget = nbytes < minim_heap.budget ? minim_heap.budget - nbytes : 0;
    minim_heap.allocated += nbytes;
    return minim_space_alloc(&minim_heap.spaces[MINIM_SPACE_NEW], nbytes);
}

void minim_space_free(minim_space *s) {
    minim_chunk *c = s->first;
    while (c != NULL) {
        minim_chunk *next = c->next;
        minim_heap.size -= chunk_bytes(c->size);
        chunk_free(c);
        c = next;
    }
    *s = (minim_space){.generation = s->generation};
}

size_t minim_space_used(const minim_space *s) {
    size_t used = 0;
    for (const minim_chunk *c = s->first; c != NULL; c = c->next)
        used += (size_t)((c == s->last ? s->alloc : c->end) - c->data);
    return used;
}

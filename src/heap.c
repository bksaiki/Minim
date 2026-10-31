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

#include "gc.h"
#include "minim.h"

#ifdef MINIM_ASAN
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(p, n) ((void)(p), (void)(n))
#define ASAN_UNPOISON_MEMORY_REGION(p, n) ((void)(p), (void)(n))
#endif

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

// A spare chunk, past the quarantine, or NULL.
static minim_chunk *take_spare(void) {
    minim_chunk_queue *q = &minim_gc.spares;
    if (q->len <= MINIM_CHUNK_QUARANTINE)
        return NULL;
    minim_chunk *c = q->first;
    q->first = c->next;
    if (q->first == NULL)
        q->last = NULL;
    q->len--;
    return c;
}

// Spares kept: enough to refill the nursery, plus the quarantine.
static size_t max_spares(void) {
    return minim_gc.config.nursery_bytes / MINIM_CHUNK_SIZE + 1 + MINIM_CHUNK_QUARANTINE;
}

// Makes `c` a spare, or frees it if it is large or there are enough spares.
static void release_chunk(minim_chunk *c) {
    minim_chunk_queue *q = &minim_gc.spares;
    if (chunk_bytes(c->size) != MINIM_CHUNK_SIZE || q->len >= max_spares()) {
        minim_gc.size -= chunk_bytes(c->size);
        chunk_free(c);
        return;
    }
    ASAN_POISON_MEMORY_REGION(c->data, c->size);
    c->next = NULL;
    if (q->last != NULL)
        q->last->next = c;
    else
        q->first = c;
    q->last = c;
    q->len++;
}

// Closes the last chunk of `s` and appends one that fits `nbytes`.  Its data
// stays poisoned until allocated.
static void space_grow(minim_space *s, size_t nbytes) {
    // A multiple of MINIM_CHUNK_SIZE, as aligned_alloc requires.
    size_t bytes = (chunk_bytes(nbytes) + MINIM_CHUNK_SIZE - 1) & ~(size_t)(MINIM_CHUNK_SIZE - 1);
    minim_chunk *c = bytes == MINIM_CHUNK_SIZE ? take_spare() : NULL;
    if (c == NULL) {
        c = chunk_alloc(bytes);
        if (c == NULL)
            minim_out_of_memory();
        minim_gc.size += bytes;
    }
    size_t size = bytes - offsetof(minim_chunk, data);
    *c = (minim_chunk){.generation = s->generation, .size = size};
    ASAN_POISON_MEMORY_REGION(c->data, size);

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
}

void *minim_space_alloc(minim_space *s, size_t nbytes) {
    nbytes = minim_align_up(nbytes);
    if (nbytes > s->avail)
        space_grow(s, nbytes);
    void *p = s->alloc;
    s->alloc += nbytes;
    s->avail -= nbytes;
    ASAN_UNPOISON_MEMORY_REGION(p, nbytes);
    return p;
}

void *minim_heap_alloc(size_t nbytes) {
    nbytes = minim_align_up(nbytes);
    if (nbytes > minim_gc.budget)
        minim_gc_collect();
    minim_gc.budget = nbytes < minim_gc.budget ? minim_gc.budget - nbytes : 0;
    minim_gc.allocated += nbytes;
    return minim_space_alloc(&minim_gc.spaces[MINIM_SPACE_NEW], nbytes);
}

void minim_space_free(minim_space *s) {
    minim_chunk *c = s->first;
    while (c != NULL) {
        minim_chunk *next = c->next;
        release_chunk(c);
        c = next;
    }
    *s = (minim_space){.generation = s->generation};
}

void minim_heap_free_spares(void) {
    minim_chunk *c = minim_gc.spares.first;
    while (c != NULL) {
        minim_chunk *next = c->next;
        minim_gc.size -= chunk_bytes(c->size);
        ASAN_UNPOISON_MEMORY_REGION(c->data, c->size);
        chunk_free(c);
        c = next;
    }
    minim_gc.spares = (minim_chunk_queue){0};
}

size_t minim_space_used(const minim_space *s) {
    size_t used = 0;
    for (const minim_chunk *c = s->first; c != NULL; c = c->next)
        used += (size_t)((c == s->last ? s->alloc : c->end) - c->data);
    return used;
}

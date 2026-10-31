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

// Bytes allocated for a chunk with `capacity` bytes of data.
static size_t chunk_bytes(size_t capacity) {
    return offsetof(minim_chunk, data) + capacity;
}

// A new chunk of `bytes`, a multiple of MINIM_CHUNK_SIZE, aligned to its size.
static minim_chunk *chunk_alloc(size_t bytes) {
#ifdef _MSC_VER
    minim_chunk *c = _aligned_malloc(bytes, MINIM_CHUNK_SIZE);
#else
    minim_chunk *c = aligned_alloc(MINIM_CHUNK_SIZE, bytes);
#endif
    if (c == NULL)
        minim_out_of_memory();
    minim_gc.size += bytes;
    return c;
}

static void chunk_free(minim_chunk *c) {
    minim_gc.size -= chunk_bytes(c->size);
#ifdef _MSC_VER
    _aligned_free(c);
#else
    free(c);
#endif
}

// Takes the oldest spare, or NULL if none is past the quarantine.
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

size_t minim_heap_max_spares(void) {
    size_t in_use = minim_gc.size / MINIM_CHUNK_SIZE - minim_gc.spares.len;
    return in_use + minim_gc.config.nursery_bytes / MINIM_CHUNK_SIZE + 1 + MINIM_CHUNK_QUARANTINE;
}

// Makes `c` a spare, or frees it if it is large.
static void release_chunk(minim_chunk *c) {
    minim_chunk_queue *q = &minim_gc.spares;
    if (chunk_bytes(c->size) != MINIM_CHUNK_SIZE) {
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
    if (c == NULL)
        c = chunk_alloc(bytes);
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
    // A large object's chunk holds nothing else.
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

void minim_heap_trim_spares(void) {
    while (minim_gc.spares.len > minim_heap_max_spares())
        chunk_free(take_spare());
}

void minim_heap_free_spares(void) {
    minim_chunk *c = minim_gc.spares.first;
    while (c != NULL) {
        minim_chunk *next = c->next;
        chunk_free(c);
        c = next;
    }
    minim_gc.spares = (minim_chunk_queue){0};
}

size_t minim_space_used(const minim_space *s) {
    size_t used = 0;
    for (const minim_chunk *c = s->first; c != NULL; c = c->next)
        used += (size_t)(minim_chunk_end(s, c) - c->data);
    return used;
}

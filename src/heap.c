#include "heap.h"

#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>

static_assert(alignof(max_align_t) >= MINIM_ALIGN, "malloc must return 16-byte aligned memory");

typedef struct minim_chunk {
    struct minim_chunk *next;
    alignas(MINIM_ALIGN) unsigned char data[];
} minim_chunk;

// The new area: a chain of chunks with one bump pointer.
static struct minim_heap {
    minim_chunk *chunks;   // newest first
    unsigned char *alloc;  // bump pointer into the current chunk
    size_t avail;          // bytes left in the current chunk
    size_t allocated;
} heap;

static minim_chunk *new_chunk(size_t size) {
    minim_chunk *c = malloc(sizeof(minim_chunk) + size);
    if (c == NULL) {
        fprintf(stderr, "minim: out of memory\n");
        abort();
    }
    c->next = heap.chunks;
    heap.chunks = c;
    return c;
}

void minim_heap_init(void) {
    heap = (struct minim_heap){0};
}

void minim_heap_shutdown(void) {
    minim_chunk *c = heap.chunks;
    while (c != NULL) {
        minim_chunk *next = c->next;
        free(c);
        c = next;
    }
    heap = (struct minim_heap){0};
}

void *minim_heap_alloc(size_t nbytes) {
    nbytes = (nbytes + MINIM_ALIGN - 1) & ~(size_t)(MINIM_ALIGN - 1);
    heap.allocated += nbytes;

    // Large objects get their own chunk; the current chunk stays in use.
    if (nbytes > MINIM_CHUNK_SIZE)
        return new_chunk(nbytes)->data;

    if (nbytes > heap.avail) {
        heap.alloc = new_chunk(MINIM_CHUNK_SIZE)->data;
        heap.avail = MINIM_CHUNK_SIZE;
    }
    void *p = heap.alloc;
    heap.alloc += nbytes;
    heap.avail -= nbytes;
    return p;
}

size_t minim_heap_allocated(void) {
    return heap.allocated;
}

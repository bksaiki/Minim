#include "heap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "minim.h"

static_assert(alignof(max_align_t) >= MINIM_ALIGN, "malloc must return 16-byte aligned memory");

struct minim_heap minim_heap;

void minim_heap_init(void) {
    minim_heap = (struct minim_heap){0};
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

// Closes the last chunk of `s` and appends one that fits `nbytes`.
static void space_grow(minim_space *s, size_t nbytes) {
    size_t size = nbytes > MINIM_CHUNK_SIZE ? nbytes : MINIM_CHUNK_SIZE;
    minim_chunk *c = malloc(sizeof(minim_chunk) + size);
    if (c == NULL)
        minim_out_of_memory();
    *c = (minim_chunk){.size = size};

    if (s->last != NULL) {
        s->last->end = s->alloc;
        s->last->next = c;
    } else {
        s->first = c;
    }
    s->last = c;
    s->alloc = c->data;
    s->avail = size;
    minim_heap.size += size;
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
    minim_gc_collect();
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
        minim_heap.size -= c->size;
        free(c);
        c = next;
    }
    *s = (minim_space){0};
}

size_t minim_space_used(const minim_space *s) {
    size_t used = 0;
    for (const minim_chunk *c = s->first; c != NULL; c = c->next)
        used += (size_t)((c == s->last ? s->alloc : c->end) - c->data);
    return used;
}

#ifndef MINIM_HEAP_H
#define MINIM_HEAP_H

#include <stddef.h>

enum {
    MINIM_ALIGN = 16,
    MINIM_CHUNK_SIZE = 64 * 1024,
};

void minim_heap_init(void);
void minim_heap_shutdown(void);

// Returns MINIM_ALIGN-aligned memory of `nbytes` rounded up to MINIM_ALIGN.
// Aborts when out of memory.
void *minim_heap_alloc(size_t nbytes);

// Total bytes handed out since minim_heap_init.
size_t minim_heap_allocated(void);

#endif

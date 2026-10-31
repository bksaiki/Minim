#ifndef MINIM_HEAP_H
#define MINIM_HEAP_H

#include <stdalign.h>
#include <stddef.h>

enum {
    MINIM_ALIGN = 16,
    MINIM_CHUNK_SIZE = 64 * 1024,
};

static inline size_t minim_align_up(size_t n) {
    return (n + MINIM_ALIGN - 1) & ~(size_t)(MINIM_ALIGN - 1);
}

typedef struct minim_chunk {
    struct minim_chunk *next;  // next newer chunk
    unsigned char *end;        // end of used data, once a newer chunk exists
    size_t size;               // capacity of `data`
    alignas(MINIM_ALIGN) unsigned char data[];
} minim_chunk;

// Chunks in allocation order, with a bump pointer into the last one.
typedef struct minim_space {
    minim_chunk *first;
    minim_chunk *last;
    unsigned char *alloc;
    size_t avail;
} minim_space;

// New objects go in MINIM_SPACE_NEW; the collector copies survivors into the
// space for their kind.
enum {
    MINIM_SPACE_NEW,
    MINIM_SPACE_PAIR,
    MINIM_SPACE_SYMBOL,
    MINIM_SPACE_TYPED,
    MINIM_SPACE_COUNT,
};

extern struct minim_heap {
    minim_space spaces[MINIM_SPACE_COUNT];
    size_t size;       // bytes held in chunks
    size_t allocated;  // bytes handed out by minim_heap_alloc since init
    size_t live;       // bytes surviving the last collection
    size_t budget;     // bytes minim_heap_alloc may hand out before collecting
} minim_heap;

void minim_heap_init(void);
void minim_heap_shutdown(void);

_Noreturn void minim_out_of_memory(void);

// Allocates `nbytes`, rounded up to MINIM_ALIGN, in the new area.  May
// collect first.
void *minim_heap_alloc(size_t nbytes);

// Allocates `nbytes`, rounded up to MINIM_ALIGN, in `s`.  Never collects.
void *minim_space_alloc(minim_space *s, size_t nbytes);

// Frees every chunk in `s` and empties it.
void minim_space_free(minim_space *s);

// Bytes in use in `s`.
size_t minim_space_used(const minim_space *s);

#endif

#include "gc.h"

#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "minim.h"

enum {
    MIN_BUDGET = 1024 * 1024,  // bytes
    GROWTH = 2,                // budget is GROWTH * live bytes
};

typedef struct root_stack {
    minim_obj **items;
    size_t len;
    size_t cap;
} root_stack;

static root_stack locals;  // the shadow stack
static root_stack globals;

static void push(root_stack *s, minim_obj *p) {
    if (s->len == s->cap) {
        s->cap = s->cap ? 2 * s->cap : 64;
        s->items = realloc(s->items, s->cap * sizeof(*s->items));
        if (s->items == NULL)
            minim_out_of_memory();
    }
    s->items[s->len++] = p;
}

void minim_gc_init(void) {
    minim_heap.budget = MIN_BUDGET;
}

void minim_gc_shutdown(void) {
    free(locals.items);
    free(globals.items);
    locals = (root_stack){0};
    globals = (root_stack){0};
}

size_t minim_gc_mark(void) {
    return locals.len;
}

void minim_gc_reset(size_t mark) {
    assert(mark <= locals.len);
    locals.len = mark;
}

void minim_gc_protect(minim_obj *const *vars, size_t n) {
    for (size_t i = 0; i < n; i++)
        push(&locals, vars[i]);
}

void minim_gc_add_global(minim_obj *p) {
    push(&globals, p);
}

static size_t typed_size(minim_obj header) {
    size_t n = (size_t)(header >> MINIM_HEADER_LENGTH_SHIFT);
    switch (header & MINIM_HEADER_TYPE_MASK) {
    case MINIM_TYPE_VECTOR:
        return minim_vector_size(n);
    default:
        abort();  // unknown type
    }
}

// Copies the object `x` refers to into its to-space, unless already copied,
// and returns the new reference.
static minim_obj copy(minim_obj x) {
    minim_obj tag = minim_tag(x);
    if (tag != MINIM_TAG_PAIR && tag != MINIM_TAG_TYPED)
        return x;

    minim_obj *old = (minim_obj *)(x - tag);
    if (old[0] == MINIM_FORWARD)
        return old[1];

    size_t size;
    minim_space *to;
    if (tag == MINIM_TAG_PAIR) {
        size = MINIM_PAIR_SIZE;
        to = &minim_heap.spaces[MINIM_SPACE_PAIR];
    } else {
        size = typed_size(old[0]);
        to = &minim_heap.spaces[MINIM_SPACE_TYPED];
    }

    void *p = minim_space_alloc(to, size);
    memcpy(p, old, size);
    minim_obj y = (minim_obj)p | tag;
    old[0] = MINIM_FORWARD;
    old[1] = y;
    return y;
}

// Copies the referents of the object at `p`; returns its size.
static size_t scan_pair(minim_obj *p) {
    p[0] = copy(p[0]);
    p[1] = copy(p[1]);
    return MINIM_PAIR_SIZE;
}

static size_t scan_typed(minim_obj *p) {
    minim_obj header = p[0];
    size_t n = (size_t)(header >> MINIM_HEADER_LENGTH_SHIFT);
    switch (header & MINIM_HEADER_TYPE_MASK) {
    case MINIM_TYPE_VECTOR:
        for (size_t i = 0; i < n; i++)
            p[1 + i] = copy(p[1 + i]);
        break;
    default:
        abort();  // unknown type
    }
    return typed_size(header);
}

// Position of the next unscanned object in a space
typedef struct cursor {
    minim_chunk *chunk;
    unsigned char *p;
} cursor;

static unsigned char *chunk_end(const minim_space *s, const minim_chunk *c) {
    return c == s->last ? s->alloc : c->end;
}

// Scans `s` from `c` to its end; returns whether anything was scanned.
static bool scan(minim_space *s, cursor *c, size_t (*scan_obj)(minim_obj *)) {
    if (c->chunk == NULL) {
        if (s->first == NULL)
            return false;
        *c = (cursor){s->first, s->first->data};
    }

    bool scanned = false;
    for (;;) {
        // Scanning may copy into `s`, so the end is rechecked each time.
        while (c->p < chunk_end(s, c->chunk)) {
            c->p += minim_align_up(scan_obj((minim_obj *)c->p));
            scanned = true;
        }
        if (c->chunk == s->last)
            return scanned;
        c->chunk = c->chunk->next;
        c->p = c->chunk->data;
    }
}

void minim_gc_collect(void) {
    minim_space from[MINIM_SPACE_COUNT];
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        from[i] = minim_heap.spaces[i];
        minim_heap.spaces[i] = (minim_space){0};
    }

    for (size_t i = 0; i < globals.len; i++)
        *globals.items[i] = copy(*globals.items[i]);
    for (size_t i = 0; i < locals.len; i++)
        *locals.items[i] = copy(*locals.items[i]);

    minim_space *pairs = &minim_heap.spaces[MINIM_SPACE_PAIR];
    minim_space *typed = &minim_heap.spaces[MINIM_SPACE_TYPED];
    cursor pair_cursor = {0};
    cursor typed_cursor = {0};
    bool scanned;
    do {
        scanned = scan(pairs, &pair_cursor, scan_pair);
        scanned |= scan(typed, &typed_cursor, scan_typed);
    } while (scanned);

    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++)
        minim_space_free(&from[i]);

    minim_heap.live = minim_space_used(pairs) + minim_space_used(typed);
    size_t budget = GROWTH * minim_heap.live;
    minim_heap.budget = budget > MIN_BUDGET ? budget : MIN_BUDGET;
}

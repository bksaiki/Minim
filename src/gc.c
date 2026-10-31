/**
 * @file
 * Precise generational copying collector (Cheney) and root stacks.
 */

#include "gc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"
#include "heap.h"
#include "minim.h"

enum {
    DEFAULT_NURSERY = 1024 * 1024,     // bytes
    MIN_MAJOR_LIMIT = 1024 * 1024,  // bytes
    GROWTH = 2,                     // the old generation's growth that triggers a major collection
};

static minim_gc_policy policy;
static size_t scheduled;    // calls to minim_gc_collect
static size_t major_limit;  // old-generation bytes that trigger a major collection
static bool minor;          // whether the collection in progress is minor

typedef struct root_stack {
    minim_obj **items;
    size_t len;
    size_t cap;
} root_stack;

static root_stack locals;  // the shadow stack
static root_stack globals;

// Old objects written since the last collection, each once.
static struct {
    minim_obj *items;
    size_t len;
    size_t cap;
} remembered;

struct minim_values minim_values;

static void push(root_stack *s, minim_obj *p) {
    if (s->len == s->cap) {
        s->cap = s->cap ? 2 * s->cap : 64;
        s->items = realloc(s->items, s->cap * sizeof(*s->items));
        if (s->items == NULL)
            minim_out_of_memory();
    }
    s->items[s->len++] = p;
}

// Value of the environment variable `name`, a decimal integer from `min` to
// the largest fixnum, or `fallback` if it is unset.  Exits if it is malformed.
static size_t getenv_integer(const char *name, size_t min, const char *expected, size_t fallback) {
    const char *s = minim_getenv(name);
    if (s == NULL)
        return fallback;
    char *end;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (*s < '0' || *s > '9' || *end != '\0' || errno == ERANGE || v < min
        || v > MINIM_FIXNUM_MAX) {
        fprintf(stderr, "minim: %s must be %s, not \"%s\"\n", name, expected, s);
        exit(EXIT_FAILURE);
    }
    return (size_t)v;
}

void minim_gc_init(void) {
    policy = (minim_gc_policy){
        .nursery_bytes = getenv_integer("MINIM_GC_NURSERY", 1, "a positive integer", DEFAULT_NURSERY),
        .radix = getenv_integer("MINIM_GC_RADIX", 0, "a nonnegative integer", 0),
    };
    scheduled = 0;
    major_limit = MIN_MAJOR_LIMIT;
    minim_heap.budget = policy.nursery_bytes;
}

minim_gc_policy minim_gc_get_policy(void) {
    return policy;
}

void minim_gc_set_policy(minim_gc_policy p) {
    policy = p;
    minim_heap.budget = policy.nursery_bytes;
}

void minim_gc_shutdown(void) {
    free(locals.items);
    free(globals.items);
    free(minim_values.items);
    free(remembered.items);
    locals = (root_stack){0};
    globals = (root_stack){0};
    minim_values = (struct minim_values){0};
    remembered.items = NULL;
    remembered.len = remembered.cap = 0;
}

// The word holding `obj`'s remembered bit, and the bit in `*bit`.
static uint64_t *remembered_word(minim_obj obj, uint64_t *bit) {
    minim_chunk *c = minim_chunk_of(obj);
    size_t i = ((obj & ~(minim_obj)MINIM_TAG_MASK) - (uintptr_t)c) / MINIM_ALIGN;
    *bit = (uint64_t)1 << (i % 64);
    return &c->remembered[i / 64];
}

void minim_gc_remember(minim_obj obj) {
    uint64_t bit;
    uint64_t *word = remembered_word(obj, &bit);
    if (*word & bit)
        return;
    *word |= bit;
    if (remembered.len == remembered.cap) {
        remembered.cap = remembered.cap ? 2 * remembered.cap : 64;
        remembered.items = realloc(remembered.items, remembered.cap * sizeof(minim_obj));
        if (remembered.items == NULL)
            minim_out_of_memory();
    }
    remembered.items[remembered.len++] = obj;
}

size_t minim_gc_remembered(void) {
    return remembered.len;
}

void minim_values_push(minim_obj x) {
    if (minim_values.len == minim_values.cap) {
        minim_values.cap = minim_values.cap ? 2 * minim_values.cap : 64;
        minim_values.items = realloc(minim_values.items, minim_values.cap * sizeof(minim_obj));
        if (minim_values.items == NULL)
            minim_out_of_memory();
    }
    minim_values.items[minim_values.len++] = x;
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
    size_t n = minim_header_length(header);
    switch (minim_header_type(header)) {
    case MINIM_TYPE_VECTOR:
        return minim_vector_size(n);
    case MINIM_TYPE_STRING:
        return minim_string_size(n);
    case MINIM_TYPE_PRIMITIVE:
        return MINIM_PRIMITIVE_SIZE;
    default:
        abort();  // unknown type
    }
}

// Copies the object `x` refers to into its to-space, unless already copied,
// and returns the new reference.
static minim_obj copy(minim_obj x) {
    minim_obj tag = minim_tag(x);
    minim_space *to;
    switch (tag) {
    case MINIM_TAG_PAIR:
    case MINIM_TAG_CLOSURE:
        to = &minim_heap.spaces[MINIM_SPACE_PAIR];
        break;
    case MINIM_TAG_SYMBOL:
        to = &minim_heap.spaces[MINIM_SPACE_SYMBOL];
        break;
    case MINIM_TAG_TYPED:
        to = &minim_heap.spaces[MINIM_SPACE_TYPED];
        break;
    default:
        return x;  // not a heap object
    }

    if (minor && minim_chunk_of(x)->generation != 0)
        return x;  // old objects stay put in a minor collection

    minim_obj *old = (minim_obj *)(x - tag);
    if (old[0] == MINIM_FORWARD)
        return old[1];

    size_t size = tag == MINIM_TAG_TYPED ? typed_size(old[0]) : 2 * sizeof(minim_obj);
    void *p = minim_space_alloc(to, size);
    memcpy(p, old, size);
    minim_obj y = (minim_obj)p | tag;
    old[0] = MINIM_FORWARD;
    old[1] = y;
    return y;
}

// Copies the referents of the object at `p`; returns its size.  Pairs,
// symbols, and closures are two traced words.
static size_t scan_two_words(minim_obj *p) {
    p[0] = copy(p[0]);
    p[1] = copy(p[1]);
    return 2 * sizeof(minim_obj);
}

static size_t scan_typed(minim_obj *p) {
    minim_obj header = p[0];
    size_t n = minim_header_length(header);
    switch (minim_header_type(header)) {
    case MINIM_TYPE_VECTOR:
        for (size_t i = 0; i < n; i++)
            p[1 + i] = copy(p[1 + i]);
        break;
    case MINIM_TYPE_STRING:
        break;  // no pointers
    case MINIM_TYPE_PRIMITIVE:
        p[2] = copy(p[2]);  // name; `fn` is a C pointer, the arities fixnums
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

// Scans the object `x`, which hasn't moved: copies its referents.
static void scan_object(minim_obj x) {
    minim_obj *p = (minim_obj *)(x - minim_tag(x));
    if (minim_tag(x) == MINIM_TAG_TYPED)
        scan_typed(p);
    else
        scan_two_words(p);
}

// A cursor at the end of `s`, where objects copied next will start.
static cursor end_of(const minim_space *s) {
    return s->last == NULL ? (cursor){0} : (cursor){s->last, s->alloc};
}

// Copies the young generation's survivors into the old generation, and in a
// major collection also the old generation's.  A minor collection treats the
// remembered old objects as roots.
static void collect(bool is_minor) {
    minor = is_minor;
    minim_space from[MINIM_SPACE_COUNT];
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        if (minor && i != MINIM_SPACE_NEW)
            continue;
        from[i] = minim_heap.spaces[i];
        minim_heap.spaces[i] = (minim_space){.generation = from[i].generation};
    }

    // Objects copied from here on are scanned; in a major collection, that is
    // every object in the old spaces.
    minim_space *pairs = &minim_heap.spaces[MINIM_SPACE_PAIR];
    minim_space *symbols = &minim_heap.spaces[MINIM_SPACE_SYMBOL];
    minim_space *typed = &minim_heap.spaces[MINIM_SPACE_TYPED];
    cursor pair_cursor = end_of(pairs);
    cursor symbol_cursor = end_of(symbols);
    cursor typed_cursor = end_of(typed);

    for (size_t i = 0; i < globals.len; i++)
        *globals.items[i] = copy(*globals.items[i]);
    for (size_t i = 0; i < locals.len; i++)
        *locals.items[i] = copy(*locals.items[i]);
    for (size_t i = 0; i < minim_values.len; i++)
        minim_values.items[i] = copy(minim_values.items[i]);

    // In a major collection, the remembered objects are in from-space, and
    // their chunks are freed below, bits and all.
    if (minor) {
        for (size_t i = 0; i < remembered.len; i++) {
            uint64_t bit;
            *remembered_word(remembered.items[i], &bit) &= ~bit;
            scan_object(remembered.items[i]);
        }
    }
    remembered.len = 0;
    bool scanned;
    do {
        scanned = scan(pairs, &pair_cursor, scan_two_words);
        scanned |= scan(symbols, &symbol_cursor, scan_two_words);
        scanned |= scan(typed, &typed_cursor, scan_typed);
    } while (scanned);

    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        if (!minor || i == MINIM_SPACE_NEW)
            minim_space_free(&from[i]);
    }

    minim_heap.live = minim_space_used(pairs) + minim_space_used(symbols)
                    + minim_space_used(typed);
    minim_heap.budget = policy.nursery_bytes;
    if (minor) {
        minim_heap.minor_collections++;
    } else {
        minim_heap.major_collections++;
        size_t limit = GROWTH * minim_heap.live;
        major_limit = limit > MIN_MAJOR_LIMIT ? limit : MIN_MAJOR_LIMIT;
    }
}

void minim_gc_collect_minor(void) {
    collect(true);
}

void minim_gc_collect_major(void) {
    collect(false);
}

void minim_gc_collect(void) {
    scheduled++;
    if (policy.radix != 0 && scheduled % policy.radix == 0) {
        collect(false);
        return;
    }
    collect(true);
    if (minim_heap.live > major_limit)
        collect(false);
}

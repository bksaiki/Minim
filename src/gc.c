/**
 * @file
 * Precise generational copying collector (Cheney) and root stacks.
 */

#include "gc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "minim.h"
#include "system.h"

static const minim_gc_config DEFAULT_CONFIG = {
    .nursery_bytes = 1024 * 1024,
    .radix = 0,
    .major_growth = 2,
    .major_min_bytes = 1024 * 1024,
    .stack_initial_capacity = 64,
};

struct minim_gc minim_gc;

static void push_root(struct minim_gc *gc, minim_root_stack *s, minim_obj *p) {
    if (s->len == s->cap) {
        s->cap = s->cap ? 2 * s->cap : gc->config.stack_initial_capacity;
        s->items = realloc(s->items, s->cap * sizeof(*s->items));
        if (s->items == NULL)
            minim_out_of_memory();
    }
    s->items[s->len++] = p;
}

static void push_obj(struct minim_gc *gc, minim_obj_stack *s, minim_obj x) {
    if (s->len == s->cap) {
        s->cap = s->cap ? 2 * s->cap : gc->config.stack_initial_capacity;
        s->items = realloc(s->items, s->cap * sizeof(*s->items));
        if (s->items == NULL)
            minim_out_of_memory();
    }
    s->items[s->len++] = x;
}

// Value of the environment variable `name`, a decimal integer from `min` to
// `max`, or `fallback` if it is unset.  Exits if it is malformed or out of range.
static size_t getenv_integer(const char *name, size_t min, size_t max, size_t fallback) {
    const char *s = minim_getenv(name);
    if (s == NULL)
        return fallback;
    char *end;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (*s < '0' || *s > '9' || *end != '\0' || errno == ERANGE || v < min || v > max) {
        fprintf(stderr, "minim: %s must be an integer from %zu to %zu, not \"%s\"\n", name, min,
                max, s);
        exit(EXIT_FAILURE);
    }
    return (size_t)v;
}

void minim_gc_init(void) {
    const size_t max = MINIM_FIXNUM_MAX;
    minim_gc_config env = {
        .nursery_bytes = getenv_integer("MINIM_GC_NURSERY_BYTES", 1, max,
                                        DEFAULT_CONFIG.nursery_bytes),
        .radix = getenv_integer("MINIM_GC_RADIX", 0, max, DEFAULT_CONFIG.radix),
        .major_growth = getenv_integer("MINIM_GC_MAJOR_GROWTH", 1, max,
                                       DEFAULT_CONFIG.major_growth),
        .major_min_bytes = getenv_integer("MINIM_GC_MAJOR_MIN_BYTES", 0, max,
                                          DEFAULT_CONFIG.major_min_bytes),
        .stack_initial_capacity = DEFAULT_CONFIG.stack_initial_capacity,
    };
    bool startup = getenv_integer("MINIM_GC_STARTUP", 0, 1, 0);

    minim_gc = (struct minim_gc){.config = startup ? env : DEFAULT_CONFIG, .program_config = env};
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++)
        minim_gc.spaces[i].generation = i == MINIM_SPACE_NEW ? 0 : 1;
    minim_gc.budget = minim_gc.config.nursery_bytes;
}

void minim_gc_finish_startup(void) {
    minim_gc_set_config(minim_gc.program_config);
}

void minim_gc_shutdown(void) {
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++)
        minim_space_free(&minim_gc.spaces[i]);
    minim_heap_free_spares();
    free(minim_gc.locals.items);
    free(minim_gc.globals.items);
    free(minim_gc.values.items);
    free(minim_gc.remembered.items);
    minim_gc = (struct minim_gc){0};
}

minim_gc_config minim_gc_default_config(void) {
    return DEFAULT_CONFIG;
}

minim_gc_config minim_gc_get_config(void) {
    return minim_gc.config;
}

void minim_gc_set_config(minim_gc_config config) {
    assert(config.major_growth >= 1 && config.stack_initial_capacity >= 1);
    minim_gc.config = config;
    minim_gc.budget = config.nursery_bytes;
}

// The word holding `obj`'s remembered bit; sets `*bit` to its mask.
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
    push_obj(&minim_gc, &minim_gc.remembered, obj);
}

void minim_values_push(minim_obj x) {
    push_obj(&minim_gc, &minim_gc.values, x);
}

size_t minim_gc_mark(void) {
    return minim_gc.locals.len;
}

void minim_gc_reset(size_t mark) {
    assert(mark <= minim_gc.locals.len);
    minim_gc.locals.len = mark;
}

void minim_gc_protect(minim_obj *const *vars, size_t n) {
    for (size_t i = 0; i < n; i++)
        push_root(&minim_gc, &minim_gc.locals, vars[i]);
}

void minim_gc_add_global(minim_obj *p) {
    push_root(&minim_gc, &minim_gc.globals, p);
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
        to = &minim_gc.spaces[MINIM_SPACE_PAIR];
        break;
    case MINIM_TAG_SYMBOL:
        to = &minim_gc.spaces[MINIM_SPACE_SYMBOL];
        break;
    case MINIM_TAG_TYPED:
        to = &minim_gc.spaces[MINIM_SPACE_TYPED];
        break;
    default:
        return x;  // not a heap object
    }

    if (minim_gc.minor && minim_chunk_of(x)->generation != 0)
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

// Position of the next unscanned object in a space.
typedef struct cursor {
    minim_chunk *chunk;
    unsigned char *p;
} cursor;

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
        while (c->p < minim_chunk_end(s, c->chunk)) {
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
static void collect(bool minor) {
    minim_gc.minor = minor;
    minim_space from[MINIM_SPACE_COUNT];
    cursor cursors[MINIM_SPACE_COUNT];  // of the old spaces
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        if (!minim_gc.minor || i == MINIM_SPACE_NEW) {
            from[i] = minim_gc.spaces[i];
            minim_gc.spaces[i] = (minim_space){.generation = from[i].generation};
        }
        // Objects copied from here on are scanned; in a major collection, that
        // is every object in the old spaces.
        cursors[i] = end_of(&minim_gc.spaces[i]);
    }

    for (size_t i = 0; i < minim_gc.globals.len; i++)
        *minim_gc.globals.items[i] = copy(*minim_gc.globals.items[i]);
    for (size_t i = 0; i < minim_gc.locals.len; i++)
        *minim_gc.locals.items[i] = copy(*minim_gc.locals.items[i]);
    for (size_t i = 0; i < minim_gc.values.len; i++)
        minim_gc.values.items[i] = copy(minim_gc.values.items[i]);

    // In a major collection, the remembered objects are in from-space, and
    // their chunks are freed below, bits and all.
    if (minim_gc.minor) {
        for (size_t i = 0; i < minim_gc.remembered.len; i++) {
            uint64_t bit;
            uint64_t *word = remembered_word(minim_gc.remembered.items[i], &bit);
            *word &= ~bit;
            scan_object(minim_gc.remembered.items[i]);
        }
    }
    minim_gc.remembered.len = 0;
    bool scanned;
    do {
        scanned = false;
        for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
            if (i != MINIM_SPACE_NEW)
                scanned |= scan(&minim_gc.spaces[i], &cursors[i],
                                i == MINIM_SPACE_TYPED ? scan_typed : scan_two_words);
        }
    } while (scanned);

    minim_gc.live = 0;
    for (size_t i = 0; i < MINIM_SPACE_COUNT; i++) {
        if (!minim_gc.minor || i == MINIM_SPACE_NEW)
            minim_space_free(&from[i]);
        if (i != MINIM_SPACE_NEW)
            minim_gc.live += minim_space_used(&minim_gc.spaces[i]);
    }
    minim_heap_trim_spares();
    minim_gc.budget = minim_gc.config.nursery_bytes;
    if (minim_gc.minor) {
        minim_gc.minor_collections++;
    } else {
        minim_gc.major_collections++;
        minim_gc.major_base = minim_gc.live;
    }
}

void minim_gc_collect_minor(void) {
    collect(true);
}

void minim_gc_collect_major(void) {
    collect(false);
}

// Whether the old generation has outgrown its configured limit.
static bool old_generation_outgrown(void) {
    size_t live = minim_gc.live;
    const minim_gc_config *c = &minim_gc.config;
    // live > major_growth * major_base without overflow; live >= 1 by the first test
    return live > c->major_min_bytes && (live - 1) / c->major_growth >= minim_gc.major_base;
}

void minim_gc_collect(void) {
    minim_gc.scheduled++;
    if (minim_gc.config.radix != 0 && minim_gc.scheduled % minim_gc.config.radix == 0) {
        collect(false);
        return;
    }
    collect(true);
    if (old_generation_outgrown())
        collect(false);
}

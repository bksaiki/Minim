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
#include "stack.h"
#include "system.h"

static const minim_gc_config DEFAULT_CONFIG = {
    .nursery_bytes = 1024 * 1024,
    .radix = 4,
    .max_generation = 4,
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
    minim_gc_config config = {
        .nursery_bytes = getenv_integer("MINIM_GC_NURSERY_BYTES", 1, max,
                                        DEFAULT_CONFIG.nursery_bytes),
        .radix = getenv_integer("MINIM_GC_RADIX", 1, max, DEFAULT_CONFIG.radix),
        .max_generation = getenv_integer("MINIM_GC_MAX_GENERATION", 1, MINIM_MAX_GENERATION,
                                         DEFAULT_CONFIG.max_generation),
        .stack_initial_capacity = DEFAULT_CONFIG.stack_initial_capacity,
    };
    minim_gc = (struct minim_gc){.config = config};
    minim_gc.values.items = malloc(MINIM_SEGMENT_SIZE * sizeof(minim_obj));
    if (minim_gc.values.items == NULL)
        minim_out_of_memory();
    minim_gc.values.below = MINIM_NIL;
    for (unsigned g = 1; g <= MINIM_MAX_GENERATION; g++) {
        for (size_t k = 0; k < MINIM_KIND_COUNT; k++)
            minim_gc.old[g][k].generation = (unsigned char)g;
    }
    minim_gc.budget = minim_gc.config.nursery_bytes;
}

void minim_gc_shutdown(void) {
    minim_space_free(&minim_gc.nursery);
    for (unsigned g = 1; g <= MINIM_MAX_GENERATION; g++) {
        for (size_t k = 0; k < MINIM_KIND_COUNT; k++)
            minim_space_free(&minim_gc.old[g][k]);
    }
    minim_heap_free_spares();
    free(minim_gc.locals.items);
    free(minim_gc.globals.items);
    free(minim_gc.values.items);
    free(minim_gc.spill.items);
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
    assert(config.nursery_bytes >= 1 && config.radix >= 1 && config.stack_initial_capacity >= 1);
    assert(config.max_generation >= 1 && config.max_generation <= MINIM_MAX_GENERATION);
    minim_gc.config = config;
    minim_gc.scheduled = 0;
    minim_gc.budget = config.nursery_bytes;
}

// `obj`'s byte in its chunk's `remembered`.
static unsigned char *remembered_byte(minim_obj obj) {
    minim_chunk *c = minim_chunk_of(obj);
    return &c->remembered[((obj & ~(minim_obj)MINIM_TAG_MASK) - (uintptr_t)c) / MINIM_ALIGN];
}

void minim_gc_remember(minim_obj obj) {
    unsigned char *r = remembered_byte(obj);
    if (*r == 0)
        push_obj(&minim_gc, &minim_gc.remembered, obj);
    *r = 1;  // may point to generation 0
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
    case MINIM_TYPE_CONTINUATION:
        return MINIM_CONTINUATION_SIZE;
    default:
        abort();  // unknown type
    }
}

// Size of the object at `p` with the tag `tag`.
static size_t object_size(const minim_obj *p, minim_obj tag) {
    return tag == MINIM_TAG_TYPED ? typed_size(p[0]) : 2 * sizeof(minim_obj);
}

// Copies the object `x` refers to into the target generation, unless it is
// older than the collection or already copied, and returns the new reference.
static minim_obj copy(minim_obj x) {
    minim_obj tag = minim_tag(x);
    size_t kind;
    switch (tag) {
    case MINIM_TAG_PAIR:
    case MINIM_TAG_CLOSURE:
        kind = MINIM_KIND_PAIR;
        break;
    case MINIM_TAG_SYMBOL:
        kind = MINIM_KIND_SYMBOL;
        break;
    case MINIM_TAG_TYPED:
        kind = MINIM_KIND_TYPED;
        break;
    default:
        return x;  // not a heap object
    }

    if (minim_chunk_of(x)->generation > minim_gc.collecting)
        return x;

    minim_obj *old = (minim_obj *)(x - tag);
    if (old[0] == MINIM_FORWARD)
        return old[1];

    size_t size = object_size(old, tag);
    void *p = minim_space_alloc(&minim_gc.old[minim_gc.target][kind], size);
    minim_gc.old_bytes[minim_gc.target] += minim_align_up(size);
    memcpy(p, old, size);
    minim_obj y = (minim_obj)p | tag;
    old[0] = MINIM_FORWARD;
    old[1] = y;
    return y;
}

// The fields of the object `x` that may hold pointers, in `*fields`; returns
// how many.
static size_t pointer_fields(minim_obj x, minim_obj **fields) {
    minim_obj *p = (minim_obj *)(x - minim_tag(x));
    if (minim_tag(x) != MINIM_TAG_TYPED) {
        *fields = p;
        return 2;
    }
    switch (minim_header_type(p[0])) {
    case MINIM_TYPE_VECTOR:
        *fields = p + 1;
        return minim_header_length(p[0]);
    case MINIM_TYPE_PRIMITIVE:
        *fields = p + 2;  // the name
        return 1;
    case MINIM_TYPE_CONTINUATION:
        *fields = p + 1;  // the frames and segments; the height is a fixnum
        return 2;
    default:
        return 0;
    }
}

// Scans the object `x`, which hasn't moved: copies its referents.
static void scan_object(minim_obj x) {
    minim_obj *fields;
    size_t n = pointer_fields(x, &fields);
    for (size_t i = 0; i < n; i++)
        fields[i] = copy(fields[i]);
}

// Position of the next unscanned object in a space.
typedef struct cursor {
    minim_chunk *chunk;
    unsigned char *p;
} cursor;

// Scans `s`, the space for `kind`, from `c` to its end; returns whether
// anything was scanned.
static bool scan(minim_space *s, size_t kind, cursor *c) {
    // Pairs, symbols, and closures all have two pointer fields.
    minim_obj tag = kind == MINIM_KIND_TYPED ? MINIM_TAG_TYPED : MINIM_TAG_PAIR;
    if (c->chunk == NULL) {
        if (s->first == NULL)
            return false;
        *c = (cursor){s->first, s->first->data};
    }

    bool scanned = false;
    for (;;) {
        // Scanning may copy into `s`, so the end is rechecked each time.
        while (c->p < minim_chunk_end(s, c->chunk)) {
            minim_obj *p = (minim_obj *)c->p;
            scan_object((minim_obj)p | tag);
            c->p += minim_align_up(object_size(p, tag));
            scanned = true;
        }
        if (c->chunk == s->last)
            return scanned;
        c->chunk = c->chunk->next;
        c->p = c->chunk->data;
    }
}

// A cursor at the end of `s`, where objects copied next will start.
static cursor end_of(const minim_space *s) {
    return s->last == NULL ? (cursor){0} : (cursor){s->last, s->alloc};
}

// The youngest generation the old object `x` points to, or its own.
static unsigned youngest_referent(minim_obj x) {
    unsigned youngest = minim_chunk_of(x)->generation;
    minim_obj *fields;
    size_t n = pointer_fields(x, &fields);
    for (size_t i = 0; i < n; i++) {
        minim_obj tag = minim_tag(fields[i]);
        if (tag != MINIM_TAG_FIXNUM && tag != MINIM_TAG_IMMEDIATE
            && minim_chunk_of(fields[i])->generation < youngest)
            youngest = minim_chunk_of(fields[i])->generation;
    }
    return youngest;
}

// The oldest generation.
static unsigned oldest(void) {
    return (unsigned)minim_gc.config.max_generation;
}

// Collects generations 0 through `g`, copying their survivors into `g + 1`.
// Collecting the oldest collects every generation present into fresh spaces
// for it, so lowering max_generation merges the ones above.  The remembered
// objects in older generations that may point into them are roots.
static void collect(unsigned g) {
    bool full = g == oldest();
    unsigned last = full ? MINIM_MAX_GENERATION : g;  // the oldest collected
    unsigned t = full ? g : g + 1;
    minim_gc.collecting = last;
    minim_gc.target = t;

    minim_space nursery = minim_gc.nursery;
    minim_space from[MINIM_MAX_GENERATION + 1][MINIM_KIND_COUNT];
    minim_gc.nursery = (minim_space){.generation = 0};
    for (unsigned i = 1; i <= last; i++) {
        for (size_t k = 0; k < MINIM_KIND_COUNT; k++) {
            from[i][k] = minim_gc.old[i][k];
            minim_gc.old[i][k] = (minim_space){.generation = (unsigned char)i};
        }
        minim_gc.old_bytes[i] = 0;  // counted again as `copy` fills the target
    }
    // Objects copied from here on are scanned.
    cursor cursors[MINIM_KIND_COUNT];
    for (size_t k = 0; k < MINIM_KIND_COUNT; k++)
        cursors[k] = end_of(&minim_gc.old[t][k]);

    for (size_t i = 0; i < minim_gc.globals.len; i++)
        *minim_gc.globals.items[i] = copy(*minim_gc.globals.items[i]);
    for (size_t i = 0; i < minim_gc.locals.len; i++)
        *minim_gc.locals.items[i] = copy(*minim_gc.locals.items[i]);
    for (size_t i = 0; i < minim_gc.values.len; i++)
        minim_gc.values.items[i] = copy(minim_gc.values.items[i]);
    minim_gc.values.below = copy(minim_gc.values.below);
    for (size_t i = 0; i < minim_gc.spill.len; i++)
        minim_gc.spill.items[i] = copy(minim_gc.spill.items[i]);

    for (size_t i = 0; i < minim_gc.remembered.len; i++) {
        minim_obj x = minim_gc.remembered.items[i];
        if (minim_chunk_of(x)->generation > last && *remembered_byte(x) - 1u <= last)
            scan_object(x);
    }
    bool scanned;
    do {
        scanned = false;
        for (size_t k = 0; k < MINIM_KIND_COUNT; k++)
            scanned |= scan(&minim_gc.old[t][k], k, &cursors[k]);
    } while (scanned);

    // Update the scanned objects' youngest referents, dropping those that no
    // longer point younger, and the moved ones, whose referents moved with them.
    size_t kept = 0;
    for (size_t i = 0; i < minim_gc.remembered.len; i++) {
        minim_obj x = minim_gc.remembered.items[i];
        unsigned char *r = remembered_byte(x);
        if (*r - 1u <= last) {
            unsigned generation = minim_chunk_of(x)->generation;
            unsigned youngest = generation > last ? youngest_referent(x) : generation;
            *r = youngest < generation ? (unsigned char)(youngest + 1) : 0;
        }
        if (*r != 0)
            minim_gc.remembered.items[kept++] = x;
    }
    minim_gc.remembered.len = kept;

    minim_space_free(&nursery);
    for (unsigned i = 1; i <= last; i++) {
        for (size_t k = 0; k < MINIM_KIND_COUNT; k++)
            minim_space_free(&from[i][k]);
    }
    minim_gc.live = 0;
    for (unsigned i = 1; i <= MINIM_MAX_GENERATION; i++)
        minim_gc.live += minim_gc.old_bytes[i];
    minim_heap_trim_spares();
    minim_gc.budget = minim_gc.config.nursery_bytes;
    minim_gc.collections[g]++;
}

void minim_gc_collect_generation(unsigned g) {
    assert(g <= oldest());
    collect(g);
}

void minim_gc_collect_minor(void) {
    collect(0);
}

void minim_gc_collect_major(void) {
    collect(oldest());
}

// The oldest generation the `n`-th scheduled collection collects: the largest
// g up to the oldest such that radix^g divides `n`.
static unsigned scheduled_generation(size_t n) {
    size_t radix = minim_gc.config.radix;
    unsigned g = 0;
    while (g < oldest() && n % radix == 0) {
        n /= radix;
        g++;
    }
    return g;
}

void minim_gc_collect(void) {
    collect(scheduled_generation(++minim_gc.scheduled));
}

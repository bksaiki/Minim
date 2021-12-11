#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#if GC_DEBUG
#include <stdio.h>
#endif

#include "gc-impl.h"

#define POINTER_SIZE        sizeof(void*)

// GC record table 
static size_t bucket_sizes[] = {
    4099,
    8209,
    16411,
    32771,
    65537,
    131101,
    262147,
    524309,
    1048583,
    2097169,
    4194319,
    8388617,
    16777259,
    33554467,
    67108879,
    134217757,
    268435459,
    536870923,
    1073741827,
    2147483659,
    4294967311,
    8589934609,
    17179869209,
    34359738421,
    68719476767,
    137438953481,
    274877906951,
    549755813911,
    1099511627791,
    2199023255579,
    4398046511119,
    8796093022237,
    17592186044423,
    35184372088891,
    70368744177679,
    140737488355333,
    281474976710677,
    562949953421381,
    1125899906842679,
    2251799813685269,
    4503599627370517,
    9007199254740997,
    18014398509482143,
    36028797018963971,
    72057594037928017,
    144115188075855881,
    288230376151711813,
    576460752303423619,
    1152921504606847009,
    2305843009213693967,
    4611686018427388039,
    0
};

void GC_atomic_mrk(void (*)(void*, void*), void*, void*);

/*************** Helper functions *****************/

#define gc_load(gc)     (gc_load2((gc)->size, (gc)->alloc))
#define gc_load2(s, a)  (((double) (s)) / ((double) (a)))
#define gc_hash(ptr)    (((size_t) ptr) >> 3)

#define ptr_arr_ref(arr, n)     (((void**) (arr))[n])
#define ptr_ref(p)              (*((void**) (p)))

// Inserts an existing bucket into the record table updating `b->next`.
static gc_record_t *
insert_record(gc_t *gc,
              gc_bucket_t *b) {
    size_t i;
    
    // compute index and insert
    i = b->record.hash % gc->alloc;
    b->next = gc->buckets[i];
    gc->buckets[i] = b;
    return &b->record;
}

// Creates a new record, adds it to the table and returns
// a pointer to it. Does not update any gc stats.
static gc_record_t *
add_record(gc_t *gc,
           void *ptr,
           size_t size,
           gc_dtor_t dtor,
           gc_mark_t mrk,
           int root) {
    gc_bucket_t *nb;

    // create record
    nb = (gc_bucket_t *) malloc(sizeof(gc_bucket_t));
    nb->record.ptr = ptr;
    nb->record.dtor = dtor;
    nb->record.mrk = mrk;
    nb->record.size = size;
    nb->record.hash = gc_hash(ptr);
    nb->record.flags = (root ? GC_BLOCK_ROOT : 0x0);

    // insert and return record
    return insert_record(gc, nb);
}

static void
remove_record(gc_t *gc,
              size_t i,
              gc_bucket_t *pb,
              gc_bucket_t *b,
              gc_bucket_t *nb) {
    gc_record_t *r = &b->record;

    // call dtor if needed and free record and bucket entry
    if (r->dtor)    r->dtor(r->ptr);
    free(r->ptr);
    free(b);

    // repair bucket entries as needed
    if (pb)     pb->next = nb;
    else        gc->buckets[i] = nb;
}

static void
gc_resize_if_needed(gc_t *gc) {
    if (gc_load(gc) > GC_TABLE_LOAD_FACTOR) {      // needs larger size
        gc_bucket_t **old_buckets = gc->buckets;
        size_t old_alloc = gc->alloc;

        // update size and create new buckets
        ++gc->alloc_ptr;
        gc->alloc = *gc->alloc_ptr;
        gc->buckets = (gc_bucket_t **) calloc(gc->alloc, sizeof(gc_bucket_t*));

        // rehash
        for (size_t i = 0; i < old_alloc; ++i) {
            gc_bucket_t *b = old_buckets[i];
            while (b) {
                gc_bucket_t *nb = b->next;
                insert_record(gc, b);
                b = nb;
            }
        }

        free(old_buckets);
    }
}

static void
gc_mark_ptr(gc_t *gc,
            void *ptr) {
    size_t i;

// #if GC_HEAP_HEURISTIC
//     // decent heuristic for possible "heap" location
//     if (ptr < (void*) gc_mark_ptr_young || ptr > gc->stack_bottom)
//         return;
// #endif

    // early exit: above stack start
    if (ptr > gc->stack_bottom)
        return;

    i = gc_hash(ptr) % gc->alloc;
    for (gc_bucket_t *b = gc->buckets[i]; b; b = b->next) {
        gc_record_t *r = &b->record;
        if (ptr == r->ptr) {    // found correct record
            if (r->flags & GC_BLOCK_MARK ||     // early exit if root or already marked
                r->flags & GC_BLOCK_ROOT)
                return;

            // set mark flag
            r->flags |= GC_BLOCK_MARK;

            // recurse into pointer block
            if (!r->mrk) {                                      // default (conservative)
                for (size_t k = 0; k < r->size / POINTER_SIZE; ++k)
                    gc_mark_ptr(gc, ptr_arr_ref(r->ptr, k));
            } else if (r->mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                r->mrk(gc_mark_ptr, gc, r->ptr);
            }

            return;
        }
    }
}

__attribute__((no_sanitize("address")))
static void
gc_mark_stack(gc_t *gc) {
    void *b, *t;

    b = gc->stack_bottom;
    t = &b;
    for (void *p = t; p <= b; p += POINTER_SIZE)
        gc_mark_ptr(gc, ptr_ref(p));
}

static void
gc_mark(gc_t *gc) {
    jmp_buf env;
    void (*volatile mark_stack)(gc_t*) = gc_mark_stack;

    // exit if nothing allocated
    if (gc->size == 0)
        return;

    // mark roots
    for (size_t i = 0; i < gc->alloc; ++i) {
        for (gc_bucket_t *b = gc->buckets[i]; b; b = b->next) {
            gc_record_t *r = &b->record;
            if (r->flags & GC_BLOCK_ROOT) {
                if (!r->mrk) {                                      // default (conservative)
                    for (size_t k = 0; k < r->size / POINTER_SIZE; ++k)
                        gc_mark_ptr(gc, ptr_arr_ref(r->ptr, k));
                } else if (r->mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                    r->mrk(gc_mark_ptr, gc, r->ptr);
                }
            }
        }
    }

    // push registers and mark stack
    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env);
    mark_stack(gc);
}

static void
gc_unmark(gc_t *gc) {
    for (size_t i = 0; i < gc->alloc; ++i) {
        for (gc_bucket_t *b = gc->buckets[i]; b; b = b->next)
            b->record.flags &= ~GC_BLOCK_MARK;
    }
}

static void
gc_sweep(gc_t *gc) {
    for (size_t i = 0; i < gc->alloc; ++i) {
        gc_bucket_t *b = gc->buckets[i], *pb = NULL;
        while (b) {
            gc_record_t *r = &b->record;
            if (!(r->flags & GC_BLOCK_MARK)) {      // unmarked
                gc_bucket_t *nb = b->next;

                // update stats and remove record
                gc->allocs -= r->size;
                --gc->size;
                remove_record(gc, i, pb, b, nb);
                
                b = nb;
            } else {
                pb = b;
                b = b->next;
            }
        }
    }

    // reset dirty byte counter and unmark
    gc->dirty = 0;
    gc_unmark(gc);    
}

#define gc_collect_if_needed(gc)                        \
{                                                       \
    if (((gc)->flags & GC_COLLECT) &&                   \
        ((gc)->dirty > GC_MIN_AUTO_COLLECT_SIZE))       \
        gc_collect(gc);                                 \
}

/*************** Interface ******************/

gc_t *
gc_create(void *stack) {
    gc_t *gc = malloc(sizeof(gc_t));
    gc->stack_bottom = stack;
    gc->alloc_ptr = &bucket_sizes[0];
    gc->alloc = *gc->alloc_ptr;
    gc->buckets = (gc_bucket_t **) calloc(gc->alloc, sizeof(gc_bucket_t*));
    gc->size = 0;
    gc->dirty = 0;
    gc->allocs = 0;
    gc->flags = GC_COLLECT;

    return gc;
}

void
gc_destroy(gc_t* gc) {
    // unmark and sweep
    gc_unmark(gc);
    gc_sweep(gc);

    // free up gc structure
    free(gc->buckets);
    free(gc);
}

void
gc_pause(gc_t *gc) {
    gc->flags &= ~GC_COLLECT;
}

void
gc_resume(gc_t *gc){
    gc->flags |= GC_COLLECT;
}

void
gc_add(gc_t *gc,
       void *ptr,
       size_t size,
       gc_dtor_t dtor,
       gc_mark_t mrk) {
    // resize and add
    gc_resize_if_needed(gc);
    add_record(gc, ptr, size, dtor, mrk, 0);

    // update stats
    gc->dirty += size;
    gc->allocs += size;
    ++gc->size;

    // collect if needed
    gc_collect_if_needed(gc);
}

void
gc_remove(gc_t *gc,
          void *ptr,
          int destroy) {
    gc_bucket_t *b, *pb;
    size_t i;

    i = gc_hash(ptr) % gc->alloc;
    b = gc->buckets[i];
    pb = NULL;
    while (b) {
        gc_record_t *r = &b->record;
        if (ptr == r->ptr) {    // found correct record
            // update stats
            gc->allocs -= r->size;
            --gc->size;

            // destroy resources as needed
            if (destroy)    remove_record(gc, i, pb, b, b->next);
            return;
        }

        b = b->next;
    }
}

gc_record_t *
gc_get_record(gc_t *gc,
              void *ptr) {
    size_t i = gc_hash(ptr) % gc->alloc;
    for (gc_bucket_t *b = gc->buckets[i]; b; b = b->next) {
        if (ptr == b->record.ptr) {
            return &b->record;
        }
    }

    return NULL;
}

void
gc_update_record(gc_t *gc,
                 gc_record_t *record,
                 size_t size,
                 gc_dtor_t dtor,
                 gc_mark_t mrk) {
    gc->allocs -= record->size;
    gc->allocs += size;

    record->size = size;
    record->dtor = dtor;
    record->mrk = mrk;
}

void
gc_collect(gc_t *gc) {
    gc_mark(gc);
    gc_sweep(gc);
}

void
gc_register_dtor(gc_t *gc,
                 void *ptr,
                 gc_dtor_t dtor) {
    gc_record_t *r = gc_get_record(gc, ptr);
    if (r)  r->dtor = dtor;
}

void
gc_register_mrk(gc_t *gc,
                void *ptr,
                gc_mark_t mrk) {
    gc_record_t *r = gc_get_record(gc, ptr);
    if (r)  r->mrk = mrk;
}

void
gc_register_root(gc_t *gc,
                 void *ptr) {
    gc_record_t *r = gc_get_record(gc, ptr);
    if (r)  r->flags |= GC_BLOCK_ROOT;
}

size_t
gc_get_allocated(gc_t *gc) {
    return gc->allocs;
}

size_t
gc_get_reachable(gc_t *gc) {
    size_t total = 0;

    gc_mark(gc);
    for (size_t i = 0; i < gc->alloc; ++i) {
        for (gc_bucket_t *b = gc->buckets[i]; b; b = b->next) {
            gc_record_t *r = &b->record;
            if (r->flags & GC_BLOCK_MARK)
                total += r->size;
        }
    }

    gc_unmark(gc);
    return total;
}

size_t
gc_get_collectable(gc_t *gc) {
    return (gc->alloc - gc_get_reachable(gc));
}

// Signals to mark phase not to descend
void GC_atomic_mrk(void (*func)(void*, void*), void* gc, void* ptr) {
    return;
}

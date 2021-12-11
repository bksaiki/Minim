#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#if GC_DEBUG
#include <stdio.h>
#endif

#include "gc-impl.h"

#define POINTER_SIZE        sizeof(void*)

static size_t bucket_sizes[] = {
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

#define ptr_arr_ref(arr, n)     (((void**) (arr))[n])
#define ptr_ref(p)              (*((void**) (p)))

// Inserts an existing bucket into the record table updating `r->next`.
static void
insert_record(gc_t *gc,
              gc_record_t *r) {
    size_t i;
    
    // compute index and insert
    i = gc_hash(r->ptr) % gc->alloc;
    gc_record_next_set(r, gc->buckets[i]);
    gc->buckets[i] = r;
}

// Creates a new record.
static gc_record_t *
make_record(void *ptr,
            size_t size,
            gc_dtor_t dtor,
            gc_mark_t mrk) {
    gc_record_t *r = (gc_record_t *) malloc(sizeof(gc_record_t));
    r->next = NULL;
    r->size = size;
    r->dtor = dtor;
    r->mrk = mrk;
    r->ptr = ptr;

    return r;
}

static void
gc_resize(gc_t *gc,
          size_t *alloc_ptr) {
    gc_record_t **old_buckets = gc->buckets;
    size_t old_alloc = gc->alloc;

    // update size and create new buckets
    gc->alloc_ptr = alloc_ptr;
    gc->alloc = *gc->alloc_ptr;
    gc->buckets = (gc_record_t **) calloc(gc->alloc, sizeof(gc_record_t*));

    // rehash
    for (size_t i = 0; i < old_alloc; ++i) {
        gc_record_t *r = old_buckets[i];
        while (r) {
            gc_record_t *nr = gc_record_next(r);     
            insert_record(gc, r);
            r = nr;
        }
    }

    free(old_buckets);
}

static void
gc_expand_if_needed(gc_t *gc) {
    if (gc_load(gc) > GC_TABLE_LOAD_FACTOR) {      // needs larger size
        gc_resize(gc, gc->alloc_ptr + 1);
    }
}

static void
gc_shrink_if_needed(gc_t *gc) {
    size_t *ideal_ptr = gc->alloc_ptr;
    // compute ideal size
    while ((*ideal_ptr > 4 * gc->size) && ideal_ptr != &bucket_sizes[0])
        --ideal_ptr;

    if (ideal_ptr != gc->alloc_ptr)
        gc_resize(gc, ideal_ptr);
}

static void
gc_mark_ptr(gc_t *gc,
            void *ptr) {
    size_t i;

#if GC_HEAP_HEURISTIC
    // decent heuristic for possible "heap" location
    if (ptr < (void*) gc_mark_ptr || ptr > gc->stack_bottom)
        return;
#endif

    i = gc_hash(ptr) % gc->alloc;
    for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r)) {
        if (ptr == r->ptr) {    // found correct record
            if (gc_record_markp(r) ||     // early exit if root or already marked
                gc_record_rootp(r))
                return;

            // set mark flag
            gc_record_mark_set(r);

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
        for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r)) {
            if (gc_record_rootp(r)) {
                gc_record_mark_set(r);
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
        for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r))
            gc_record_mark_unset(r);
    }
}

static void
gc_sweep(gc_t *gc) {
    for (size_t i = 0; i < gc->alloc; ++i) {
        gc_record_t *r = gc->buckets[i], *pr = NULL;
        while (r) {
            if (!gc_record_markp(r)) {      // unmarked
                gc_record_t *nr = gc_record_next(r);

                // update stats and remove record
                gc->allocs -= r->size;
                --gc->size;
                
                // call dtor if needed and free record and bucket entry
                if (r->dtor)    r->dtor(r->ptr);
                free(r->ptr);
                free(r);

                // repair bucket entries as needed
                if (pr)     gc_record_next_set(pr, nr);
                else        gc->buckets[i] = nr;
                
                r = nr;
            } else {
                pr = r;
                r = gc_record_next(r);
            }
        }
    }

    // reset dirty byte counter and unmark
    gc->dirty = 0;
    gc_unmark(gc);
    gc_shrink_if_needed(gc);
}

/*************** Interface ******************/

gc_t *
gc_create(void *stack) {
    gc_t *gc = malloc(sizeof(gc_t));
    gc->stack_bottom = stack;
    gc->alloc_ptr = &bucket_sizes[0];
    gc->alloc = *gc->alloc_ptr;
    gc->buckets = (gc_record_t **) calloc(gc->alloc, sizeof(gc_record_t*));
    gc->size = 0;
    gc->dirty = 0;
    gc->allocs = 0;
    gc->flags = GC_COLLECT;

    return gc;
}

void
gc_destroy(gc_t* gc) {
    // remove root flags
    for (size_t i = 0; i < gc->alloc; ++i) {
        for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r)) {
            if (gc_record_rootp(r))
                gc_record_root_unset(r);
        }
    }
    
    // free up gc structure
    gc_sweep(gc);
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
    gc_record_t *r;

    // resize and add
    gc_expand_if_needed(gc);
    r = make_record(ptr, size, dtor, mrk);
    insert_record(gc, r);

    // update stats
    gc->dirty += size;
    gc->allocs += size;
    ++gc->size;
}

void
gc_remove(gc_t *gc,
          void *ptr,
          int destroy) {
    gc_record_t *r, *pr;
    size_t i;

    i = gc_hash(ptr) % gc->alloc;
    r = gc->buckets[i];
    pr = NULL;
    while (r) {
        if (ptr == r->ptr) {    // found correct record
            // update stats
            gc->allocs -= r->size;
            --gc->size;

            // call dtor if needed and free record and bucket entry
            if (destroy) {
                if (r->dtor)    r->dtor(r->ptr);
                free(r->ptr);
            }

            // repair bucket entries as needed
            if (pr)     gc_record_next_set(pr, r->next);
            else        gc->buckets[i] = gc_record_next(r);

            free(r);
            return;
        }

        pr = r;
        r = gc_record_next(r);
    }
}

gc_record_t *
gc_alloc_record(gc_t *gc,
                size_t size,
                gc_dtor_t dtor,
                gc_mark_t mrk) {
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        gc_collect(gc);
        ptr = malloc(size);
        if (ptr == NULL)
            return NULL;
    }

    return make_record(ptr, size, dtor, mrk);
}

gc_record_t *
gc_calloc_record(gc_t *gc,
                size_t nmem,
                size_t size,
                gc_dtor_t dtor,
                gc_mark_t mrk) {
    void *ptr = calloc(nmem, size);
    if (ptr == NULL)
    {
        gc_collect(gc);
        ptr = calloc(nmem, size);
        if (ptr == NULL)
            return NULL;
    }

    return make_record(ptr, nmem * size, dtor, mrk);
}

gc_record_t *
gc_realloc_record(gc_t *gc,
                  gc_record_t *r,
                  size_t size,
                  gc_dtor_t dtor,
                  gc_mark_t mrk) {
    gc_record_t *r2;
    void *ptr2;
    
    ptr2 = realloc(r->ptr, size);
    if (ptr2 == NULL)
    {
        gc_collect(gc);
        ptr2 = realloc(r->ptr, size);
        if (ptr2 == NULL)
            return NULL;
    }

    r2 = make_record(ptr2, size, dtor, mrk);
    if (gc_record_rootp(r)) gc_record_root_set(r2);
    return r2;
}

gc_record_t *
gc_get_record(gc_t *gc,
              void *ptr) {
    size_t i = gc_hash(ptr) % gc->alloc;
    for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r)) {
        if (ptr == r->ptr) {
            return r;
        }
    }

    return NULL;
}

void
gc_add_record(gc_t *gc,
              gc_record_t *record) {
    // resize and add
    gc_expand_if_needed(gc);
    insert_record(gc, record);

    // update stats
    gc->dirty += record->size;
    gc->allocs += record->size;
    ++gc->size;
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
    if (r)  gc_record_root_set(r);
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
        for (gc_record_t *r = gc->buckets[i]; r; r = gc_record_next(r)) {
            if (gc_record_markp(r))
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
void
GC_atomic_mrk(void (*func)(void*, void*), 
              void* gc,
              void* ptr) {
    return;
}

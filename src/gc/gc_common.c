#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "gc_common.h"

#if GC_DEBUG
#include <stdio.h>
#endif

#define NUM_PRIME_SIZES     23
#define POINTER_SIZE        sizeof(void*)

static size_t PRIME_SIZES[NUM_PRIME_SIZES] = {
                        53,       97,        193,
    389,      769,      1543,     3079,      6151, 
    12289,    24593,    49157,    98137,     196613,
    393241,   786433,   1572869,  3145739,   6291469,
    12582917, 25165843, 50331653, 100663319, 201326611
};

void GC_atomic_mrk(void (*)(void*, void*), void*, void*);

/*************** Helper functions *****************/

#define gc_hash(ptr)    (((size_t) ptr) >> 3)

static size_t gc_probe(size_t i, size_t h, size_t c) {
    long n = i - (h - 1);
    return (n < 0) ? c + n : n;
}

static void gc_add_young_pool(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk, int update) {
    size_t h, i, j, p;
    gc_block_t item, t;

    i = gc_hash(ptr) % gc->youngc;
    j = 0;

    item.ptr = ptr;
    item.dtor = dtor;
    item.mrk = mrk;
    item.size = size;
    item.hash = i + 1;
    item.flags = 0x0;
    
    while (1) {
        h = gc->young[i].hash;
        if (!h) {
            gc->young[i] = item;
            if (update) {
                gc->allocs += size;
                gc->dirty += size;
                ++gc->nyoung;
            }
            return;
        }

        if (gc->young[i].ptr == item.ptr)
            return;

        p = gc_probe(i, h, gc->youngc);
        if (j >= p) {           // shift blocks (LIFO, locality)
            t = gc->young[i];
            gc->young[i] = item;
            item = t;
            j = p;
        }

        i = (i + 1) % gc->youngc;
        ++j;
    }
}

static void gc_add_old_pool(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk, int update) {
    size_t h, i, j, p;
    gc_block_t item, t;

    i = gc_hash(ptr) % gc->oldc;
    j = 0;

    item.ptr = ptr;
    item.dtor = dtor;
    item.mrk = mrk;
    item.size = size;
    item.hash = i + 1;
    item.flags = 0x0;
    
    while (1) {
        h = gc->old[i].hash;
        if (!h) {
            gc->old[i] = item;
            if (update) ++gc->nold;
            return;
        }

        if (gc->old[i].ptr == item.ptr)
            return;

        p = gc_probe(i, h, gc->oldc);
        if (j >= p) {
            t = gc->old[i];
            gc->old[i] = item;
            item = t;
            j = p;
        }

        i = (i + 1) % gc->oldc;
        ++j;
    }
}

static void gc_resize_if_needed(gc_t *gc) {
    gc_block_t *old;
    double load;
    size_t s;

    load = (double) gc->nyoung / (double) gc->youngc;
    if (load > GC_TABLE_LOAD_FACTOR) {
        for (s = 0; s < NUM_PRIME_SIZES; ++s) {
            if (gc->youngc == PRIME_SIZES[s])
                break;
        }

        old = gc->young;
        gc->young = calloc(PRIME_SIZES[s + 1], sizeof(gc_block_t));
        gc->youngc = PRIME_SIZES[s + 1];
        for (size_t i = 0; i < PRIME_SIZES[s]; ++i) {
            if (old[i].hash)
                gc_add_young_pool(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    } else if (gc->youngc > PRIME_SIZES[0] && (load < GC_TABLE_LOAD_FACTOR / 4.0)) {
        size_t n;

        for (s = 0; s < NUM_PRIME_SIZES; ++s) {
            if (gc->youngc == PRIME_SIZES[s])
                break;
        }

        for (n = s - 1; n > 0; --n) {
            if (PRIME_SIZES[n] < 4 * gc->nyoung)
                break;
        }

        old = gc->young;
        gc->young = calloc(PRIME_SIZES[n], sizeof(gc_block_t));
        gc->youngc = PRIME_SIZES[n];
        for (size_t i = 0; i < PRIME_SIZES[s]; ++i) {
            if (old[i].hash)
                gc_add_young_pool(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    }
}

static void gc_increase_old_pool(gc_t *gc, size_t incr) {
    gc_block_t *old;
    double load;
    size_t s, n;

    load = (double) (gc->nold + incr) / (double) gc->oldc;
    if (load > GC_TABLE_LOAD_FACTOR) {
        old = gc->old;
        n = gc->oldc;

        for (s = 0; PRIME_SIZES[s] < 2 * (gc->nold + incr); ++s);
        gc->old = calloc(PRIME_SIZES[s], sizeof(gc_block_t));
        gc->oldc = PRIME_SIZES[s];

        for (size_t i = 0; i < n; ++i) {
            if (old[i].hash)
                gc_add_old_pool(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    }
}

static void gc_shrink_old_pool(gc_t *gc) {
    gc_block_t *old;
    double load;
    size_t s, n;

    load = (double) gc->nold / (double) gc->oldc;
    if (gc->oldc > PRIME_SIZES[0] && (load < GC_TABLE_LOAD_FACTOR / 4.0)) {
        for (s = 0; s < NUM_PRIME_SIZES; ++s) {
            if (gc->oldc == PRIME_SIZES[s])
                break;
        }

        for (n = s - 1; n > 0; --n) {
            if (PRIME_SIZES[n] < 4 * gc->nold)
                break;
        }

        old = gc->old;
        gc->old = calloc(PRIME_SIZES[n], sizeof(gc_block_t));
        gc->oldc = PRIME_SIZES[n];
        for (size_t i = 0; i < PRIME_SIZES[s]; ++i) {
            if (old[i].hash)
                gc_add_old_pool(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    }
}

static void gc_mark_ptr_young(gc_t *gc, void *ptr) {
    size_t h, i, j;

#if GC_HEAP_HEURISTIC
    // decent heuristic for possible "heap" location
    if (ptr < (void*) gc_mark_ptr_young || ptr > gc->stack_bottom)
        return;
#endif

    i = gc_hash(ptr) % gc->youngc;
    j = 0;
    while (1) {
        h = gc->young[i].hash;
        if (!h || j > gc_probe(i, h, gc->youngc))
            return;

        if (ptr == gc->young[i].ptr) {
            if (gc->young[i].flags & GC_BLOCK_MARK)
                return;
                
            gc->young[i].flags |= GC_BLOCK_MARK;
#if GC_USE_CUSTOM_MARKERS == 2
            if (!gc->young[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_young(gc, ((void**) gc->young[i].ptr)[k]);
            } else if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->young[i].mrk(gc_mark_ptr_young, gc, gc->young[i].ptr);
            }
#elif GC_USE_CUSTOM_MARKERS == 1
            if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk)              // default (conservative)
            {
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_young(gc, ((void**) gc->young[i].ptr)[k]);
            }
#else
            for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_young(gc, ((void**) gc->young[i].ptr)[k]);
#endif

            return;
        }

        i = (i + 1) % gc->youngc;
        ++j;
    }
}

static void gc_mark_ptr_all(gc_t *gc, void *ptr) {
    size_t h, i, j;

#if GC_HEAP_HEURISTIC
    // decent heuristic for possible "heap" location
    if (ptr < (void*) gc_mark_ptr_all || ptr > gc->stack_bottom)
        return;
#endif

    // try younger generation first
    i = gc_hash(ptr) % gc->youngc;
    j = 0;
    while (1) {
        h = gc->young[i].hash;
        if (!h || j > gc_probe(i, h, gc->youngc))
            break;

        if (ptr == gc->young[i].ptr) {
            if (gc->young[i].flags & GC_BLOCK_MARK)
                return;
                
            gc->young[i].flags |= GC_BLOCK_MARK;
#if GC_USE_CUSTOM_MARKERS == 2
            if (!gc->young[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->young[i].ptr)[k]);
            } else if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->young[i].mrk(gc_mark_ptr_all, gc, gc->young[i].ptr);
            }
#elif GC_USE_CUSTOM_MARKERS == 1
            if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk)              // default (conservative)
            {
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->young[i].ptr)[k]);
            }
#else // GC_USE_CUSTOM_MARKERS == 0
            for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->young[i].ptr)[k]);
#endif

            return;
        }

        i = (i + 1) % gc->youngc;
        ++j;
    }

    // try older generation
    i = gc_hash(ptr) % gc->oldc;
    j = 0;
    while (1) {
        h = gc->old[i].hash;
        if (!h || j > gc_probe(i, h, gc->oldc))
            return;

        if (ptr == gc->old[i].ptr) {
            if (gc->old[i].flags & GC_BLOCK_MARK)
                return;
                
            gc->old[i].flags |= GC_BLOCK_MARK;
            if (!gc->old[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->old[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->old[i].ptr)[k]);
            } else if (gc->old[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->old[i].mrk(gc_mark_ptr_all, gc, gc->old[i].ptr);
            }

            return;
        }

        i = (i + 1) % gc->oldc;
        ++j;
    }
}

__attribute__((no_sanitize("address")))
static void gc_mark_stack_young(gc_t *gc) {
    void *b, *t;

    b = gc->stack_bottom; t = &b;
    for (void *i = t; i <= b; i += POINTER_SIZE)
        gc_mark_ptr_young(gc, *((void**) i));
}

__attribute__((no_sanitize("address")))
static void gc_mark_stack_all(gc_t *gc) {
    void *b, *t;

    b = gc->stack_bottom; t = &b;
    for (void *i = t; i <= b; i += POINTER_SIZE)
        gc_mark_ptr_all(gc, *((void**) i));
}

static void gc_mark_young(gc_t *gc) {
    jmp_buf env;
    void (*volatile mark_stack)(gc_t*) = gc_mark_stack_young;

    // exit if nothing allocated
    if (gc->nyoung == 0)
        return;

    // mark young roots
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash && gc->young[i].flags & GC_BLOCK_ROOT) {
            gc->young[i].flags |= GC_BLOCK_MARK;
            if (!gc->young[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_young(gc, ((void**) gc->young[i].ptr)[k]);
            } else if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->young[i].mrk(gc_mark_ptr_young, gc, gc->young[i].ptr);
            }
        }
    }

    // mark all old blocks, descent will stop if we mark old block
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash && !(gc->old[i].flags & GC_BLOCK_MARK)) {
            gc->old[i].flags |= GC_BLOCK_MARK;
            if (!gc->old[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->old[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_young(gc, ((void**) gc->old[i].ptr)[k]);
            } else if (gc->old[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->old[i].mrk(gc_mark_ptr_young, gc, gc->old[i].ptr);
            }
        }
    }

    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env);
    mark_stack(gc);
}

static void gc_mark_all(gc_t *gc) {
    jmp_buf env;
    void (*volatile mark_stack)(gc_t*) = gc_mark_stack_all;

    // exit if nothing allocated
    if (gc->nyoung == 0 && gc->nold == 0)
        return;

    // mark young roots
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash && gc->young[i].flags & GC_BLOCK_ROOT) {
            gc->young[i].flags |= GC_BLOCK_MARK;
            if (!gc->young[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->young[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->young[i].ptr)[k]);
            } else if (gc->young[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->young[i].mrk(gc_mark_ptr_all, gc, gc->young[i].ptr);
            }
        }
    }

    // mark old roots
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash && gc->old[i].flags & GC_BLOCK_ROOT) {
            gc->old[i].flags |= GC_BLOCK_MARK;
            if (!gc->old[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->old[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr_all(gc, ((void**) gc->old[i].ptr)[k]);
            } else if (gc->old[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->old[i].mrk(gc_mark_ptr_all, gc, gc->old[i].ptr);
            }
        }
    }

    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env);
    mark_stack(gc);
}

static void gc_unmark(gc_t *gc) {
    // clear young generation
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash)
            gc->young[i].flags &= ~GC_BLOCK_MARK;
    }
    // clear old generation
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash)
            gc->old[i].flags &= ~GC_BLOCK_MARK;
    }
}

static void gc_move_to_old(gc_t *gc) {
    gc_increase_old_pool(gc, gc->nyoung);
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash)
            gc_add_old_pool(gc, gc->young[i].ptr, gc->young[i].size,
                            gc->young[i].dtor, gc->young[i].mrk, 1);
    }

    gc->young = realloc(gc->young, PRIME_SIZES[0] * sizeof(gc_block_t));
    gc->youngc = PRIME_SIZES[0];
    gc->nyoung = 0;

    memset(gc->young, 0, PRIME_SIZES[0] * sizeof(gc_block_t));
}

static void gc_sweep_young(gc_t *gc) {
    gc_block_t *frees;
    size_t freec, i, j, k, nj, nh;

    freec = 0;
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (!gc->young[i].hash)                     continue;
        if (gc->young[i].flags & GC_BLOCK_MARK)     continue;
        if (gc->young[i].flags & GC_BLOCK_ROOT)     continue;
        ++freec;
    }

    frees = malloc(freec * sizeof(gc_block_t));
    if (!frees) return;

    i = 0; k = 0;
    while (i < gc->youngc && gc->nyoung > 0) {
        if (!gc->young[i].hash ||
             gc->young[i].flags & GC_BLOCK_MARK ||
             gc->young[i].flags & GC_BLOCK_ROOT) {
            ++i;
            continue;
        }

        gc->allocs -= gc->young[i].size;
        frees[k] = gc->young[i];
        memset(&gc->young[i], 0, sizeof(gc_block_t));

        --gc->nyoung;
        ++k;
        
        j = i;
        while (1) {
            nj = (j + 1) % gc->youngc;
            nh = gc->young[nj].hash;
            if (nh && gc_probe(nj, nh, gc->youngc) > 0) {
                memcpy(&gc->young[j], &gc->young[nj], sizeof(gc_block_t));
                memset(&gc->young[nj], 0, sizeof(gc_block_t));
                j = nj;
            } else {
                break;
            }
        } 
    }

    for (size_t i = 0; i < freec; ++i) {
        if (frees[i].dtor)  frees[i].dtor(frees[i].ptr);
        free(frees[i].ptr);
    }

    gc->dirty = 0;
    free(frees);
    gc_move_to_old(gc);

    // clear old generation
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash)
            gc->old[i].flags &= ~GC_BLOCK_MARK;
    }
}

static void gc_sweep_all(gc_t *gc) {
    gc_block_t *frees;
    size_t freec, i, j, k, nj, nh;

    // young generation
    freec = 0;
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (!gc->young[i].hash)                     continue;
        if (gc->young[i].flags & GC_BLOCK_MARK)     continue;
        if (gc->young[i].flags & GC_BLOCK_ROOT)     continue;
        ++freec;
    }

    // old generation
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (!gc->old[i].hash)                     continue;
        if (gc->old[i].flags & GC_BLOCK_MARK)     continue;
        if (gc->old[i].flags & GC_BLOCK_ROOT)     continue;
        ++freec;
    }

    frees = malloc(freec * sizeof(gc_block_t));
    if (!frees) return;

    // young generation
    i = 0; k = 0;
    while (i < gc->youngc && gc->nyoung > 0) {
        if (!gc->young[i].hash ||
             gc->young[i].flags & GC_BLOCK_MARK ||
             gc->young[i].flags & GC_BLOCK_ROOT) {
            ++i;
            continue;
        }

        gc->allocs -= gc->young[i].size;
        frees[k] = gc->young[i];
        memset(&gc->young[i], 0, sizeof(gc_block_t));

        --gc->nyoung;
        ++k;
        
        j = i;
        while (1) {
            nj = (j + 1) % gc->youngc;
            nh = gc->young[nj].hash;
            if (nh && gc_probe(nj, nh, gc->youngc) > 0) {
                memcpy(&gc->young[j], &gc->young[nj], sizeof(gc_block_t));
                memset(&gc->young[nj], 0, sizeof(gc_block_t));
                j = nj;
            } else {
                break;
            }
        } 
    }

    i = 0;
    while (i < gc->oldc && gc->nold > 0) {
        if (!gc->old[i].hash ||
             gc->old[i].flags & GC_BLOCK_MARK ||
             gc->old[i].flags & GC_BLOCK_ROOT) {
            ++i;
            continue;
        }

        gc->allocs -= gc->old[i].size;
        frees[k] = gc->old[i];
        memset(&gc->old[i], 0, sizeof(gc_block_t));

        --gc->nold;
        ++k;
        
        j = i;
        while (1) {
            nj = (j + 1) % gc->oldc;
            nh = gc->old[nj].hash;
            if (nh && gc_probe(nj, nh, gc->oldc) > 0) {
                memcpy(&gc->old[j], &gc->old[nj], sizeof(gc_block_t));
                memset(&gc->old[nj], 0, sizeof(gc_block_t));
                j = nj;
            } else {
                break;
            }
        } 
    }

    for (size_t i = 0; i < freec; ++i) {
        if (frees[i].dtor)  frees[i].dtor(frees[i].ptr);
        free(frees[i].ptr);
    }

    gc->dirty = 0;
    free(frees);
    gc_shrink_old_pool(gc);
    gc_move_to_old(gc);

    // clear old generation
    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash)
            gc->old[i].flags &= ~GC_BLOCK_MARK;
    }
}

#define gc_collect_if_needed(gc)                    \
{                                                   \
    if (gc->dirty > GC_MIN_AUTO_COLLECT_SIZE) {     \
        if (gc->cycles >= GC_MINOR_PER_MAJOR)       \
            gc_collect(gc);                         \
        else                                        \
            gc_collect_young(gc);                   \
    }                                               \
}

/*************** Interface ******************/

gc_t *gc_create(void *stack) {
    gc_t *gc = malloc(sizeof(gc_t));
    gc->stack_bottom = stack;
    gc->young = calloc(PRIME_SIZES[0], sizeof(gc_block_t));
    gc->youngc = PRIME_SIZES[0];
    gc->nyoung = 0;
    gc->old = calloc(PRIME_SIZES[0], sizeof(gc_block_t));
    gc->oldc = PRIME_SIZES[0];
    gc->nold = 0;
    gc->allocs = 0;
    gc->dirty = 0;
    gc->cycles = 0;

    return gc;
}

void gc_destroy(gc_t* gc) {
    gc_sweep_all(gc);
    free(gc->young);
    free(gc->old);
    free(gc);
}

void gc_add(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk) {
    gc_resize_if_needed(gc); 
    gc_add_young_pool(gc, ptr, size, dtor, mrk, 1);
    gc_collect_if_needed(gc);
}

void gc_remove(gc_t *gc, void *ptr, int destroy) {
    size_t h, j, i, nj, nh;

    // try younger generatiom
    i = gc_hash(ptr) % gc->youngc;
    j = 0;
    while (1) {
        h = gc->young[i].hash;
        if (!h || j > gc_probe(i, h, gc->youngc))
            break;

        if (gc->young[i].ptr == ptr) {
            gc->allocs -= gc->young[i].size;
            if (destroy) {
                if (gc->young[i].dtor)  gc->young[i].dtor(gc->young[i].ptr);
                free(gc->young[i].ptr);
            }

            memset(&gc->young[i], 0, sizeof(gc_block_t));
            j = i;
            while (1) {
                nj = (j + 1) % gc->youngc;
                nh = gc->young[nj].hash;
                if (nh && gc_probe(nj, nh, gc->youngc) > 0) {
                    memcpy(&gc->young[j], &gc->young[nj], sizeof(gc_block_t));
                    memset(&gc->young[nj], 0, sizeof(gc_block_t));
                    j = nj;
                } else {
                    break;
                }
            }

            --gc->nyoung;
            return;
        }

        i = (i + 1) % gc->youngc;
        ++j;
    }

    // try older generatiom
    i = gc_hash(ptr) % gc->oldc;
    j = 0;
    while (1) {
        h = gc->old[i].hash;
        if (!h || j > gc_probe(i, h, gc->oldc))
            return;

        if (gc->old[i].ptr == ptr) {
            gc->allocs -= gc->old[i].size;
            if (destroy) {
                if (gc->old[i].dtor)  gc->old[i].dtor(gc->old[i].ptr);
                free(gc->old[i].ptr);
            }

            memset(&gc->old[i], 0, sizeof(gc_block_t));
            j = i;
            while (1) {
                nj = (j + 1) % gc->oldc;
                nh = gc->old[nj].hash;
                if (nh && gc_probe(nj, nh, gc->oldc) > 0) {
                    memcpy(&gc->old[j], &gc->old[nj], sizeof(gc_block_t));
                    memset(&gc->old[nj], 0, sizeof(gc_block_t));
                    j = nj;
                } else {
                    break;
                }
            }

            --gc->nold;
            return;
        }

        i = (i + 1) % gc->oldc;
        ++j;
    }
}

gc_block_t *gc_get_block(gc_t *gc, void *ptr) {
    size_t h, i, j;

    // try young generation
    i = gc_hash(ptr) % gc->youngc;
    j = 0;
    while (1) {
        h = gc->young[i].hash;
        if (!h || j > gc_probe(i, h, gc->youngc))
            break;

        if (gc->young[i].ptr == ptr)
            return &gc->young[i];

        i = (i + 1) % gc->youngc;
        ++j;
    }

    // try old generation
    i = gc_hash(ptr) % gc->oldc;
    j = 0;
    while (1) {
        h = gc->old[i].hash;
        if (!h || j > gc_probe(i, h, gc->oldc))
            return NULL;

        if (gc->old[i].ptr == ptr)
            return &gc->old[i];

        i = (i + 1) % gc->oldc;
        ++j;
    }
}

void gc_update_block(gc_t *gc, gc_block_t *block, size_t size, gc_dtor_t dtor, gc_mark_t mrk) {
    gc->allocs -= block->size;
    gc->allocs += size;

    block->size = size;
    block->dtor = dtor;
    block->mrk = mrk;
}

void gc_collect(gc_t *gc) {
    gc_mark_all(gc);
    gc_sweep_all(gc);
    gc->cycles = 0;
}

void gc_collect_young(gc_t *gc) {
    gc_mark_young(gc);
    gc_sweep_young(gc);
    ++gc->cycles;
}

void gc_register_dtor(gc_t *gc, void *ptr, gc_dtor_t dtor) {
    gc_block_t *block;

    block = gc_get_block(gc, ptr);
    if (block)  block->dtor = dtor;
}

void gc_register_mrk(gc_t *gc, void *ptr, gc_mark_t mrk) {
    gc_block_t *block;

    block = gc_get_block(gc, ptr);
    if (block)  block->mrk = mrk;
}

void gc_register_root(gc_t *gc, void *ptr) {
    gc_block_t *block;

    block = gc_get_block(gc, ptr);
    if (block)  block->flags &= GC_BLOCK_ROOT;
}

size_t gc_get_allocated(gc_t *gc) {
    return gc->allocs;
}

size_t gc_get_reachable(gc_t *gc) {
    size_t total = 0;

    gc_mark_all(gc);
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash && gc->young[i].flags & GC_BLOCK_MARK)
            total += gc->young[i].size;
    }

    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash && gc->old[i].flags & GC_BLOCK_MARK)
            total += gc->old[i].size;
    }

    gc_unmark(gc);
    return total;
}

size_t gc_get_collectable(gc_t *gc) {
    size_t total = 0;

    gc_mark_all(gc);
    for (size_t i = 0; i < gc->youngc; ++i) {
        if (gc->young[i].hash && !(gc->young[i].flags & GC_BLOCK_MARK))
            total += gc->young[i].size;
    }

    for (size_t i = 0; i < gc->oldc; ++i) {
        if (gc->old[i].hash && !(gc->old[i].flags & GC_BLOCK_MARK))
            total += gc->old[i].size;
    }

    gc_unmark(gc);
    return total;
}

// Signals to mark phase not to descend
void GC_atomic_mrk(void (*func)(void*, void*), void* gc, void* ptr) {
    return;
}

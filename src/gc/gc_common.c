#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "gc_common.h"

#if GC_DEBUG
#include <stdio.h>
#endif

#define NUM_PRIME_SIZES     25
#define POINTER_SIZE        sizeof(void*)

static size_t PRIME_SIZES[NUM_PRIME_SIZES] = {
    11,       29,       53,       97,        193,
    389,      769,      1543,     3079,      6151, 
    12289,    24593,    49157,    98137,     196613,
    393241,   786433,   1572869,  3145739,   6291469,
    12582917, 25165843, 50331653, 100663319, 201326611
};

void GC_atomic_mrk(void (*)(void*, void*), void*, void*);

/*************** Helper functions *****************/

// static void gc_validate(gc_t *gc) {
//     size_t i;

//     for (i = 0; i < gc->itemc; ++i) {
//         if (gc->items[i].hash) {
//             if (gc->items[i].ptr < (void*) gc_validate ||
//                 gc->items[i].ptr > gc->stack_bottom) {
//                 printf("gc_validate(): block probably corrupted: %p\n", gc->items[i].ptr);
//             }
//         }
//     }
// }

static size_t gc_hash(void *ptr) {
    return ((size_t) ptr) >> 3;
}

static size_t gc_probe(gc_t *gc, size_t i, size_t h) {
    long n = i - (h - 1);
    return (n < 0) ? gc->itemc + n : n;
}

static void gc_add_no_resize(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk, int update) {
    size_t h, i, j, p;
    gc_block_t item, t;

    i = gc_hash(ptr) % gc->itemc;
    j = 0;

    item.ptr = ptr;
    item.dtor = dtor;
    item.mrk = mrk;
    item.size = size;
    item.hash = i + 1;
    item.flags = 0x0;
    
    while (1) {
        h = gc->items[i].hash;
        if (!h) {
            gc->items[i] = item;
            if (update) {
                gc->allocs += size;
                gc->dirty += size;
                ++gc->nitems;
            }
            return;
        }

        if (gc->items[i].ptr == item.ptr)
            return;

        p = gc_probe(gc, i, h);
        if (j >= p) {
            t = gc->items[i];
            gc->items[i] = item;
            item = t;
            j = p;
        }

        i = (i + 1) % gc->itemc;
        ++j;
    }
}

static void gc_resize_if_needed(gc_t *gc) {
    gc_block_t *old;
    double load;
    size_t s;

    if (gc->itemc == 0) {
        gc->items = calloc(PRIME_SIZES[0], sizeof(gc_block_t));
        gc->itemc = PRIME_SIZES[0];
        return;
    } 
    
    load = (double) gc->nitems / (double) gc->itemc;
    if (load > gc->load_factor) {
        for (s = 0; s < NUM_PRIME_SIZES; ++s) {
            if (gc->itemc == PRIME_SIZES[s])
                break;
        }

        old = gc->items;
        gc->items = calloc(PRIME_SIZES[s + 1], sizeof(gc_block_t));
        gc->itemc = PRIME_SIZES[s + 1];
        for (size_t i = 0; i < PRIME_SIZES[s]; ++i) {
            if (old[i].hash)
                gc_add_no_resize(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    } else if (gc->itemc > PRIME_SIZES[0] && (load < gc->load_factor / 4.0)) {
        size_t n;

        for (s = 0; s < NUM_PRIME_SIZES; ++s) {
            if (gc->itemc == PRIME_SIZES[s])
                break;
        }

        for (n = s - 1; n > 0; --n) {
            if (PRIME_SIZES[n] < 4 * gc->nitems)
                break;
        }

        old = gc->items;
        gc->items = calloc(PRIME_SIZES[n], sizeof(gc_block_t));
        gc->itemc = PRIME_SIZES[n];
        for (size_t i = 0; i < PRIME_SIZES[s]; ++i) {
            if (old[i].hash)
                gc_add_no_resize(gc, old[i].ptr, old[i].size, old[i].dtor, old[i].mrk, 0);
        }

        free(old);
    }
}

static void gc_mark_ptr(gc_t *gc, void *ptr) {
    size_t h, i, j;

    // decent heuristic for possible "heap" location
    if (ptr < (void*) gc_mark_ptr || ptr > gc->stack_bottom)
        return;

    i = gc_hash(ptr) % gc->itemc;
    j = 0;

    while (1) {
        h = gc->items[i].hash;
        if (!h || j > gc_probe(gc, i, h))
            return;

        if (ptr == gc->items[i].ptr) {
            if (gc->items[i].flags & GC_BLOCK_MARK)
                return;
                
            gc->items[i].flags |= GC_BLOCK_MARK;
            if (!gc->items[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->items[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr(gc, ((void**) gc->items[i].ptr)[k]);
            } else if (gc->items[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->items[i].mrk(gc_mark_ptr, gc, gc->items[i].ptr);
            }

            return;
        }

        i = (i + 1) % gc->itemc;
        ++j;
    }
}

__attribute__((no_sanitize("address")))
static void gc_mark_stack(gc_t *gc) {
    void *b, *t;

    b = gc->stack_bottom; t = &b;
    for (void *i = t; i <= b; i += POINTER_SIZE)
        gc_mark_ptr(gc, *((void**) i));
}

static void gc_mark(gc_t *gc) {
    jmp_buf env;
    void (*volatile mark_stack)(gc_t*) = gc_mark_stack;

    // exit if nothing allocated
    if (gc->nitems == 0)
        return;

    for (size_t i = 0; i < gc->itemc; ++i) {
        if (gc->items[i].hash && gc->items[i].flags & GC_BLOCK_ROOT) {
            gc->items[i].flags |= GC_BLOCK_MARK;
            if (!gc->items[i].mrk) {                                        // default (conservative)
                for (size_t k = 0; k < gc->items[i].size / POINTER_SIZE; ++k)
                    gc_mark_ptr(gc, ((void**) gc->items[i].ptr)[k]);
            } else if (gc->items[i].mrk != (gc_mark_t) GC_atomic_mrk) {    // custom marker
                gc->items[i].mrk(gc_mark_ptr, gc, gc->items[i].ptr);
            }
        }
    }

    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env);
    mark_stack(gc);
}

static void gc_sweep(gc_t *gc) {
    gc_block_t *frees;
    size_t freec, i, j, k, nj, nh;

    freec = 0;
    for (size_t i = 0; i < gc->itemc; ++i) {
        if (!gc->items[i].hash)                     continue;
        if (gc->items[i].flags & GC_BLOCK_MARK)     continue;
        if (gc->items[i].flags & GC_BLOCK_ROOT)     continue;
        ++freec;
    }

    frees = malloc(freec * sizeof(gc_block_t));
    if (!frees) return;

    i = 0; k = 0;
    while (i < gc->itemc) {
        if (!gc->items[i].hash ||
             gc->items[i].flags & GC_BLOCK_MARK ||
             gc->items[i].flags & GC_BLOCK_ROOT) {
            ++i;
            continue;
        }

        if (!gc->nitems)
            break;

        gc->allocs -= gc->items[i].size;
        frees[k] = gc->items[i];
        memset(&gc->items[i], 0, sizeof(gc_block_t));

        --gc->nitems;
        ++k;
        
        j = i;
        while (1) {
            nj = (j + 1) % gc->itemc;
            nh = gc->items[nj].hash;
            if (nh && gc_probe(gc, nj, nh) > 0) {
                memcpy(&gc->items[j], &gc->items[nj], sizeof(gc_block_t));
                memset(&gc->items[nj], 0, sizeof(gc_block_t));
                j = nj;
            } else {
                break;
            }
        } 
    }

    for (size_t i = 0; i < freec; ++i) {
        if (frees[i].dtor)  frees[i].dtor(frees[i].ptr);
        else                free(frees[i].ptr);
    }

    gc->dirty = 0;
    gc_resize_if_needed(gc);
    free(frees);

    for (size_t i = 0; i < gc->itemc; ++i) {
        if (gc->items[i].hash && (gc->items[i].flags & GC_BLOCK_MARK))
            gc->items[i].flags &= ~GC_BLOCK_MARK;
    }
}

static void gc_mark_sweep_if_needed(gc_t *gc) {
    if (gc->dirty > GC_MIN_AUTO_COLLECT_SIZE)
        gc_collect(gc);
}

/*************** Interface ******************/

gc_t *gc_create(void *stack) {
    gc_t *gc = malloc(sizeof(gc_t));
    gc->stack_bottom = stack;
    gc->items = NULL;
    gc->itemc = 0;
    gc->nitems = 0;
    gc->load_factor = GC_TABLE_LOAD_FACTOR;
    gc->allocs = 0;
    gc->dirty = 0;

    return gc;
}

void gc_destroy(gc_t* gc) {
    gc_sweep(gc);
    free(gc->items);
    free(gc);
}

void gc_add(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk) {
    gc_resize_if_needed(gc); 
    gc_add_no_resize(gc, ptr, size, dtor, mrk, 1);
    gc_mark_sweep_if_needed(gc);
}

void gc_remove(gc_t *gc, void *ptr) {
    size_t h, j, i, nj, nh;

    i = gc_hash(ptr) % gc->itemc;
    j = 0;

    while (1) {
        h = gc->items[i].hash;
        if (!h || j > gc_probe(gc, i, h)) return;

        if (gc->items[i].ptr == ptr) {
            gc->allocs -= gc->items[i].size;
            if (gc->items[i].dtor)  gc->items[i].dtor(gc->items[i].ptr);
            else                    free(gc->items[i].ptr);

            memset(&gc->items[i], 0, sizeof(gc_block_t));
            j = i;
            while (1) {
                nj = (j + 1) % gc->itemc;
                nh = gc->items[nj].hash;
                if (nh && gc_probe(gc, nj, nh) > 0) {
                    memcpy(&gc->items[j], &gc->items[nj], sizeof(gc_block_t));
                    memset(&gc->items[nj], 0, sizeof(gc_block_t));
                    j = nj;
                } else {
                    break;
                }
            }

            --gc->nitems;
            return;
        }

        i = (i + 1) % gc->itemc;
        ++j;
    }
}

void *gc_resize_ptr(gc_t *gc, void *ptr, size_t size, gc_dtor_t dtor, gc_mark_t mrk) {
    size_t h, i;

    gc_mark_sweep_if_needed(gc);
    i = gc_hash(ptr) % gc->itemc;

    while (1) {
        h = gc->items[i].hash;
        if (!h) return NULL;

        if (gc->items[i].ptr == ptr) {
            gc->allocs -= gc->items[i].size;
            gc->items[i].ptr = realloc(gc->items[i].ptr, size);
            gc->items[i].size = size;
            gc->items[i].dtor = dtor;
            gc->items[i].mrk = mrk;

            gc->allocs += size;
            gc->dirty += size;
            return gc->items[i].ptr;
        }

        i = (i + 1) % gc->itemc;
    }
}

void gc_collect(gc_t *gc) {
    gc_mark(gc);
    gc_sweep(gc);
}

void gc_register_dtor(gc_t *gc, void *ptr, gc_dtor_t dtor) {
    size_t h, i;

    i = gc_hash(ptr) % gc->itemc;
    while (1) {
        h = gc->items[i].hash;
        if (!h) return;

        if (gc->items[i].ptr == ptr) {
            gc->items[i].dtor = dtor;
            return;
        }

        i = (i + 1) % gc->itemc;
    }
}

void gc_register_mrk(gc_t *gc, void *ptr, gc_mark_t mrk) {
    size_t h, i;

    i = gc_hash(ptr) % gc->itemc;
    while (1) {
        h = gc->items[i].hash;
        if (!h) return;

        if (gc->items[i].ptr == ptr) {
            gc->items[i].mrk = mrk;
            return;
        }

        i = (i + 1) % gc->itemc;
    }
}

void gc_register_root(gc_t *gc, void *ptr) {
    size_t h, i;

    i = gc_hash(ptr) % gc->itemc;
    while (1) {
        h = gc->items[i].hash;
        if (!h) return;

        if (gc->items[i].ptr == ptr) {
            gc->items[i].flags |= GC_BLOCK_ROOT;
            return;
        }

        i = (i + 1) % gc->itemc;
    }
}

size_t gc_get_allocated(gc_t *gc) {
    return gc->allocs;
}

size_t gc_get_reachable(gc_t *gc) {
    size_t total = 0;

    gc_mark(gc);
    for (size_t i = 0; i < gc->itemc; ++i) {
        if (gc->items[i].hash && gc->items[i].flags & GC_BLOCK_MARK)
            total += gc->items[i].size;
    }

    return total;
}

size_t gc_get_collectable(gc_t *gc) {
    size_t total = 0;

    gc_mark(gc);
    for (size_t i = 0; i < gc->itemc; ++i) {
        if (gc->items[i].hash && !(gc->items[i].flags & GC_BLOCK_MARK))
            total += gc->items[i].size;
    }

    return total;
}

// Signals to mark phase not to descend
void GC_atomic_mrk(void (*func)(void*, void*), void* gc, void* ptr) {
    return;
}

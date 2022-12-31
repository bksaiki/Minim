#include <stdlib.h>
#include <stdio.h>

#include "gc-impl.h"
#include "gc.h"

static gc_t *main_gc;

void GC_init(void *stack) {
    main_gc = gc_create(stack);
}

void GC_finalize() {
    gc_destroy(main_gc);
}

void GC_pause() {
    gc_pause(main_gc);
}

void GC_resume() {
    gc_resume(main_gc);
}

void *GC_alloc(size_t size) {
    return GC_alloc_opt(size, NULL, NULL);
}

void *GC_calloc(size_t nmem, size_t size) {
    return GC_calloc_opt(nmem, size, NULL, NULL);
}

void *GC_realloc(void *ptr, size_t size) {
    return GC_realloc_opt(ptr, size, NULL, NULL);
}

void *GC_alloc_atomic(size_t size) {
    return GC_alloc_opt(size, NULL, GC_atomic_mrk);
}

void *GC_calloc_atomic(size_t nmem, size_t size) {
    return GC_calloc_opt(nmem, size, NULL, GC_atomic_mrk);
}

void *GC_realloc_atomic(void *ptr, size_t size) {
    return GC_realloc_opt(ptr, size, NULL, GC_atomic_mrk);
}

void *GC_alloc_opt(size_t size,
                   void (*dtor)(void*),
                   void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *r;

    // collect if needed
    gc_collect_if_needed(main_gc);
    
    // allocate
    r = gc_alloc_record(main_gc, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    gc_add_record(main_gc, r);
    return gc_record_ptr(r);
}

void *GC_calloc_opt(size_t nmem,
                    size_t size,
                    void (*dtor)(void*),
                    void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *r;

    // collect if needed
    gc_collect_if_needed(main_gc);
    
    // allocate
    r = gc_calloc_record(main_gc, nmem, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    gc_add_record(main_gc, r);
    return gc_record_ptr(r);
}

void *GC_realloc_opt(void *ptr,
                     size_t size,
                     void (*dtor)(void*),
                     void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *r;

    // zero size
    if (size == 0) {
        gc_remove(main_gc, ptr, 1);
        return NULL;
    }

    // no pointer
    if (ptr == NULL)
        return GC_alloc_opt(size, dtor, mrk);

    r = gc_get_record(main_gc, ptr);
    if (r) {
        gc_remove(main_gc, ptr, 0);
        r = gc_realloc_record(main_gc, r, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
        if (!r)
            return NULL;
        
        gc_add_record(main_gc, r);
        return gc_record_ptr(r);
    } else {
        return GC_alloc_opt(size, dtor, mrk);
    }
}

void GC_free(void *ptr) {
    gc_remove(main_gc, ptr, 1);
}

void GC_collect() {
    gc_collect(main_gc);
}

void GC_register_dtor(void *ptr, void (*func)(void*)) {
    gc_register_dtor(main_gc, ptr, func);
}

void GC_register_mrk(void *ptr, void (*func)(void (void*,void*),void*,void*)) {
    gc_register_mrk(main_gc, ptr, (gc_mark_t) func);
}

void GC_register_root(void *ptr) {
    gc_register_root(main_gc, ptr);
}

size_t GC_get_allocated() {
    return gc_get_allocated(main_gc);
}

size_t GC_get_reachable() {
    return gc_get_reachable(main_gc);
}
size_t GC_get_collectable() {
    return gc_get_collectable(main_gc);
}

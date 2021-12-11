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

void *GC_alloc_opt(size_t size,
                   void (*dtor)(void*),
                   void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *r;

    // collect if needed
    gc_collect_if_needed(main_gc);
    
    // allocate
    r = gc_alloc_record(main_gc, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    gc_add_record(main_gc, r);
    return r->ptr;
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
    return r->ptr;
}

void *GC_realloc_opt(void *ptr,
                     size_t size,
                     void (*dtor)(void*),
                     void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *rec, *rec2;

    // zero size
    if (size == 0) {
        gc_remove(main_gc, ptr, 1);
        return NULL;
    }

    // no pointer
    if (ptr == NULL)
        return GC_alloc_opt(size, dtor, mrk);

    rec = gc_get_record(main_gc, ptr);
    if (rec) {
        rec2 = gc_realloc_record(main_gc, rec, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
        if (rec->ptr == rec2->ptr) {
            gc_update_record(main_gc, rec, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
            free(rec2);
            return rec->ptr;
        } else {
            gc_remove(main_gc, ptr, 0);
            gc_add_record(main_gc, rec2);
            return rec2->ptr;
        }
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

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
    void *ptr;
    
    ptr = malloc(size);
    if (ptr == NULL)
    {
        gc_collect(main_gc);
        ptr = malloc(size);
        if (ptr == NULL)
            return NULL;
    }
    
    gc_add(main_gc, ptr, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    return ptr;
}

void *GC_calloc_opt(size_t nmem,
                    size_t size,
                    void (*dtor)(void*),
                    void (*mrk)(void (void*, void*), void*, void*)) {
    void *ptr;
    
    ptr = calloc(nmem, size);
    if (ptr == NULL)
    {
        gc_collect(main_gc);
        ptr = calloc(nmem, size);
        if (ptr == NULL)
            return NULL;
    }

    gc_add(main_gc, ptr, nmem * size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    return ptr;
}

void *GC_realloc_opt(void *ptr,
                     size_t size,
                     void (*dtor)(void*),
                     void (*mrk)(void (void*, void*), void*, void*)) {
    gc_record_t *rec;
    void *ptr2;

    if (!size) {
        gc_remove(main_gc, ptr, 1);
        return NULL;
    }
    
    ptr2 = realloc(ptr, size);
    if (ptr2 == NULL) {
        gc_remove(main_gc, ptr, 0);
        return NULL;
    }

    if (ptr == NULL) {
        gc_add(main_gc, ptr2, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
        return ptr2;
    }

    rec = gc_get_record(main_gc, ptr);
    if (rec) {
        if (ptr == ptr2) {
            gc_update_record(main_gc, rec, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
            return ptr;
        } else {
            gc_remove(main_gc, ptr, 0);
            gc_add(main_gc, ptr2, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
            return ptr2;
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

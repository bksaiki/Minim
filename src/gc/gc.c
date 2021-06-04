#include <stdlib.h>
#include "gc_common.h"
#include "gc.h"

static gc_t *main_gc;

void GC_init(void *stack) {
    main_gc = gc_create(stack);
}

void GC_finalize() {
    gc_destroy(main_gc);
}

void *GC_alloc(size_t size) {
    void *ptr = malloc(size);
    gc_add(main_gc, ptr, size, NULL, NULL);
    return ptr;
}

void *GC_calloc(size_t nmem, size_t size) {
    void *ptr = calloc(nmem, size);
    gc_add(main_gc, ptr, size, NULL, NULL);
    return ptr;
}

void *GC_realloc(void *ptr, size_t size) {
    if (size == 0) {
        gc_remove(main_gc, ptr);
        return NULL;
    }

    return gc_resize_ptr(main_gc, ptr, size, NULL, NULL);
}

void *GC_alloc_opt(size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*)) {
    void *ptr = malloc(size);
    gc_add(main_gc, ptr, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    return ptr;
}

void *GC_calloc_opt(size_t nmem, size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*)) {
    void *ptr = calloc(nmem, size);
    gc_add(main_gc, ptr, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
    return ptr;
}

void *GC_realloc_opt(void *ptr, size_t size, void (*dtor)(void*), void (*mrk)(void (void*, void*), void*, void*)) {
    if (size == 0) {
        gc_remove(main_gc, ptr);
        return NULL;
    }

    return gc_resize_ptr(main_gc, ptr, size, (gc_dtor_t) dtor, (gc_mark_t) mrk);
}

void GC_free(void *ptr) {
    gc_remove(main_gc, ptr);
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

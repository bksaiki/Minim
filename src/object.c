#include "minim.h"

#include "heap.h"

void minim_init(void) {
    minim_heap_init();
}

void minim_shutdown(void) {
    minim_heap_shutdown();
}

minim_obj minim_cons(minim_obj car, minim_obj cdr) {
    minim_obj *p = minim_heap_alloc(MINIM_PAIR_SIZE);
    p[0] = car;
    p[1] = cdr;
    return (minim_obj)p | MINIM_TAG_PAIR;
}

minim_obj minim_make_vector(size_t n, minim_obj fill) {
    assert(n <= SIZE_MAX / MINIM_ALIGN);  // header and byte size must not overflow
    minim_obj *p = minim_heap_alloc(minim_vector_size(n));
    p[0] = minim_make_header(n, MINIM_TYPE_VECTOR);
    for (size_t i = 0; i < n; i++)
        p[1 + i] = fill;
    return (minim_obj)p | MINIM_TAG_TYPED;
}

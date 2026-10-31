/**
 * @file
 * Runtime lifecycle and object constructors.
 */

#include "minim.h"

#include "gc.h"
#include "heap.h"
#include "symbol.h"
#include "char.h"

void minim_init(void) {
    minim_heap_init();
    minim_gc_init();
    minim_symbol_init();
}

void minim_shutdown(void) {
    minim_gc_shutdown();
    minim_heap_shutdown();
}

minim_obj minim_cons(minim_obj car, minim_obj cdr) {
    MINIM_FRAME(&car, &cdr);
    minim_obj *p = minim_heap_alloc(MINIM_PAIR_SIZE);
    p[0] = car;
    p[1] = cdr;
    MINIM_RETURN((minim_obj)p | MINIM_TAG_PAIR);
}

minim_obj minim_make_vector(size_t n, minim_obj fill) {
    assert(n <= SIZE_MAX / MINIM_ALIGN);  // header and byte size must not overflow
    MINIM_FRAME(&fill);
    minim_obj *p = minim_heap_alloc(minim_vector_size(n));
    p[0] = minim_make_header(n, MINIM_TYPE_VECTOR);
    for (size_t i = 0; i < n; i++)
        p[1 + i] = fill;
    MINIM_RETURN((minim_obj)p | MINIM_TAG_TYPED);
}

minim_obj minim_make_string(size_t n, uint32_t c) {
    assert(n <= SIZE_MAX / MINIM_ALIGN && minim_is_scalar_value(c));
    minim_obj *p = minim_heap_alloc(minim_string_size(n));
    p[0] = minim_make_header(n, MINIM_TYPE_STRING);
    uint32_t *chars = (uint32_t *)(p + 1);
    for (size_t i = 0; i < n; i++)
        chars[i] = c;
    return (minim_obj)p | MINIM_TAG_TYPED;
}

minim_obj minim_string_from_cstr(const char *s) {
    const unsigned char *u = (const unsigned char *)s;
    uint32_t c;
    size_t n = 0;
    for (const unsigned char *p = u; *p != '\0'; p += minim_utf8_decode(p, &c))
        n++;

    minim_obj str = minim_make_string(n, 0);
    uint32_t *chars = minim_string_chars(str);
    for (const unsigned char *p = u; *p != '\0'; chars++)
        p += minim_utf8_decode(p, chars);
    return str;
}

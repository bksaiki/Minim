// gc.h: shim for garbage collector library

#ifndef _MINIM_GC_SHIM_H_
#define _MINIM_GC_SHIM_H_

#include "../bdwgc/include/gc.h"

#define GC_init()                   GC_INIT()
#define GC_shutdown()               GC_deinit()

#define GC_alloc(n)                 GC_malloc(n)
#define GC_calloc(s, n)             GC_malloc((s) * (n))
#define GC_realloc(p, n)            GC_realloc(p, n)

#define GC_alloc_atomic(n)          GC_malloc_atomic(n)
#define GC_calloc_atomic(s, n)      GC_malloc_atomic((s) * (n))
#define GC_realloc_atomic(p, n)     GC_realloc(p, n)

#define GC_register_root(o)         GC_add_roots(o, (((void *) o) + sizeof(*o)))
#define GC_register_dtor(o, p)      GC_register_finalizer(o, p, 0, 0, 0)

#endif

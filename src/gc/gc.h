#ifndef _MINIM_GC_SHIM_H_
#define _MINIM_GC_SHIM_H_

#ifdef USE_MINIM_GC

#include "minim-gc/gc.h"

#else

#include "../../boehm-gc/include/gc.h"

#define GC_alloc(n)                 GC_malloc(n)
#define GC_calloc(s, n)             GC_malloc((s) * (n))
#define GC_realloc(p, n)            GC_realloc(p, n)

#define GC_alloc_atomic(n)          GC_malloc_atomic(n)
#define GC_calloc_atomic(s, n)      GC_malloc_atomic((s) * (n))
#define GC_realloc_atomic(p, n)     GC_realloc(p, n)

#define GC_register_root(o)         GC_add_roots(o, (((void *) o) + sizeof(*o)))
#define GC_register_dtor(o, p)      GC_register_finalizer(o, p, 0, 0, 0)

#define GC_init(x)          GC_init()
#define GC_finalize()       GC_deinit();
#define GC_pause()          GC_disable()
#define GC_resume()         GC_enable()

// remap without finalizer and marker
#define GC_alloc_opt(n, dtor, mrk)      GC_alloc(n)

// ignore
#define GC_REGISTER_LOCAL_ARRAY(x)

#endif

#endif

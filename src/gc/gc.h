#ifndef _MINIM_GC_SHIM_H_
#define _MINIM_GC_SHIM_H_

#ifdef USE_BOEHM_GC
#include "../../boehm-gc/include/gc.h"

#define GC_init(x)             GC_init()          
#define GC_alloc(n)            GC_malloc(n)
#define GC_alloc_atomic(n)     GC_malloc_atomic(n)
#define GC_calloc(s, n)        GC_malloc((s) * (n))
#define GC_register_root(o)    GC_REG

#define GC_REGISTER_LOCAL_ARRAY(x)
#define GC_finalize()
#define GC_pause()
#define GC_resume()

#else
#include "minimgc/gc.h"
#endif

#endif

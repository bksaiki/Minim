#ifndef _MINIM_GC_SHIM_H_
#define _MINIM_GC_SHIM_H_

#ifdef USE_BOEHM_GC
#error "unsupported"
#else
#include "minimgc/gc.h"
#endif

#endif

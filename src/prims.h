/**
 * @file
 * Primitive procedures.
 */

#ifndef MINIM_PRIMS_H
#define MINIM_PRIMS_H

#include "minim.h"

/** The `apply` primitive's function, which the evaluator recognizes. */
minim_obj minim_prim_apply(minim_obj *args, size_t n);

/** Binds the primitives as global symbol values; call after minim_symbol_init. */
void minim_prims_init(void);

#endif

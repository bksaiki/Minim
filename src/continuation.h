/**
 * @file
 * Continuations: `[header | k | stack | height]`, made and applied only by the
 * evaluator.
 */

#ifndef MINIM_CONTINUATION_H
#define MINIM_CONTINUATION_H

#include "minim.h"

/** Size in bytes of a continuation. */
enum { MINIM_CONTINUATION_SIZE = 4 * sizeof(minim_obj) };

/**
 * New continuation: the continuation frames `k`, and the value stack's frozen
 * segments `stack` holding its values below `height`.
 */
minim_obj minim_make_continuation(minim_obj k, minim_obj stack, size_t height);

/** Continuation frames of the continuation `c`. */
static inline minim_obj minim_continuation_k(minim_obj c) {
    assert(minim_is_continuation(c));
    return ((const minim_obj *)(c - MINIM_TAG_TYPED))[1];
}

/** Frozen value-stack segments of the continuation `c`. */
static inline minim_obj minim_continuation_stack(minim_obj c) {
    assert(minim_is_continuation(c));
    return ((const minim_obj *)(c - MINIM_TAG_TYPED))[2];
}

/** Value-stack height of the continuation `c`. */
static inline size_t minim_continuation_height(minim_obj c) {
    assert(minim_is_continuation(c));
    return (size_t)minim_fixnum_value(((const minim_obj *)(c - MINIM_TAG_TYPED))[3]);
}

#endif

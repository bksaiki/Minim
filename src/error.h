/**
 * @file
 * Error state lifecycle, and catches for the evaluator.  The error API is in
 * minim.h.
 */

#ifndef MINIM_ERROR_H
#define MINIM_ERROR_H

#include <setjmp.h>

#include "minim.h"

/** A catch: errors jump to `buf`. */
typedef struct minim_catch_frame {
    jmp_buf buf;
    struct minim_catch_frame *prev;
} minim_catch_frame;

/** Clears the error state; call after minim_gc_init. */
void minim_error_init(void);

/** Makes `c` the innermost catch; `setjmp(c->buf)` next. */
void minim_catch_push(minim_catch_frame *c);

/** Removes `c`, the innermost catch. */
void minim_catch_pop(minim_catch_frame *c);

/** Raises the last error again. */
_Noreturn void minim_reraise(void);

/** New error object of the last error. */
minim_obj minim_last_error_object(void);

#endif

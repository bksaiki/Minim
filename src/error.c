/**
 * @file
 * Errors: non-local exits to the innermost catch.
 */

#include "error.h"

#include <setjmp.h>
#include <stdlib.h>

#include "minim.h"

typedef struct handler {
    jmp_buf buf;
    struct handler *prev;
} handler;

static handler *current;  // innermost catch, or NULL
static const char *error_msg;
static minim_obj error_irritant;

void minim_error_init(void) {
    current = NULL;
    error_msg = NULL;
    error_irritant = MINIM_VOID;
    minim_gc_add_global(&error_irritant);
}

void minim_error(const char *msg, minim_obj irritant) {
    error_msg = msg;
    error_irritant = irritant;
    if (current == NULL) {
        fprintf(stderr, "error: %s", msg);
        if (irritant != MINIM_VOID) {
            fputs(": ", stderr);
            minim_write(stderr, irritant);
        }
        fputc('\n', stderr);
        abort();
    }
    longjmp(current->buf, 1);
}

bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result) {
    handler h = {.prev = current};
    size_t mark = minim_gc_mark();
    current = &h;
    if (setjmp(h.buf) != 0) {
        current = h.prev;
        minim_gc_reset(mark);
        return false;
    }

    minim_obj r = fn(arg);
    current = h.prev;
    *result = r;
    return true;
}

void minim_last_error(const char **msg, minim_obj *irritant) {
    *msg = error_msg;
    *irritant = error_irritant;
}

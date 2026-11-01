/**
 * @file
 * Errors: non-local exits to the innermost catch.
 */

#include "error.h"

#include <assert.h>
#include <stdlib.h>

#include "gc.h"
#include "minim.h"
#include "stack.h"

static minim_catch_frame *current;  // innermost catch, or NULL
static minim_error_kind error_kind;
static const char *error_msg;
static minim_obj error_irritant;

void minim_error_init(void) {
    current = NULL;
    error_msg = NULL;
    error_irritant = MINIM_VOID;
    minim_gc_add_global(&error_irritant);
}

void minim_error(const char *msg, minim_obj irritant) {
    minim_error_of_kind(MINIM_ERROR_GENERAL, msg, irritant);
}

void minim_error_of_kind(minim_error_kind kind, const char *msg, minim_obj irritant) {
    error_kind = kind;
    error_msg = msg;
    error_irritant = irritant;
    if (current == NULL) {
        minim_write_error(stderr);
        abort();
    }
    longjmp(current->buf, 1);
}

void minim_catch_push(minim_catch_frame *c) {
    c->prev = current;
    current = c;
}

void minim_catch_pop(minim_catch_frame *c) {
    assert(current == c);
    current = c->prev;
}

void minim_reraise(void) {
    minim_error_of_kind(error_kind, error_msg, error_irritant);
}

minim_obj minim_last_error_object(void) {
    minim_obj irritants = MINIM_NIL;
    MINIM_FRAME(&irritants);
    if (error_irritant != MINIM_VOID)
        irritants = minim_cons(error_irritant, MINIM_NIL);
    minim_obj msg = minim_string_from_cstr(error_msg);
    MINIM_RETURN(minim_make_error_object(error_kind, msg, irritants));
}

bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result) {
    size_t mark = minim_gc_mark();
    size_t height = minim_stack_height();
    minim_catch_frame c;
    minim_catch_push(&c);
    if (setjmp(c.buf) != 0) {
        minim_catch_pop(&c);
        minim_gc_reset(mark);
        minim_stack_unwind(height);
        minim_gc.spill.len = 0;
        return false;
    }

    minim_obj r = fn(arg);
    minim_catch_pop(&c);
    *result = r;
    return true;
}

void minim_last_error(const char **msg, minim_obj *irritant) {
    *msg = error_msg;
    *irritant = error_irritant;
}

void minim_write_error(FILE *out) {
    fprintf(out, "error: %s", error_msg);
    if (error_irritant != MINIM_VOID) {
        fputs(": ", out);
        minim_write(out, error_irritant);
    }
    fputc('\n', out);
}

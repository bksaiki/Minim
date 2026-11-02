/**
 * @file
 * Error objects, and raising errors from Scheme.
 */

#include "common.h"

#include <stdlib.h>

#include "char.h"

static minim_obj prim_make_error_object(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "%make-error-object: not a string", args[0]);
    check(minim_is_null(args[1]) || minim_is_pair(args[1]),
          "%make-error-object: not a list", args[1]);
    return minim_make_error_object(MINIM_ERROR_GENERAL, args[0], args[1]);
}

static minim_obj prim_is_error_object(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_error_object(args[0]));
}

static minim_obj prim_error_object_message(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_error_object(args[0]), "error-object-message: not an error object", args[0]);
    return minim_error_object_message(args[0]);
}

static minim_obj prim_error_object_irritants(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_error_object(args[0]), "error-object-irritants: not an error object",
          args[0]);
    return minim_error_object_irritants(args[0]);
}

static bool is_error_of_kind(minim_obj x, minim_error_kind kind) {
    return minim_is_error_object(x) && minim_error_object_kind(x) == kind;
}

static minim_obj prim_is_read_error(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(is_error_of_kind(args[0], MINIM_ERROR_READ));
}

static minim_obj prim_is_file_error(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(is_error_of_kind(args[0], MINIM_ERROR_FILE));
}

// Raises the string `msg` with the list `irritants`: void if none, the one if
// single, else the list.
static _Noreturn void raise_message(minim_obj msg, minim_obj irritants) {
    static char *message;  // must outlive the catch; replaced by the next one
    free(message);
    message = minim_string_to_utf8(msg);
    if (minim_is_null(irritants))
        minim_error(message, MINIM_VOID);
    minim_error(message, minim_is_null(minim_cdr(irritants)) ? minim_car(irritants) : irritants);
}

static minim_obj prim_syntax_error(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_obj rest = args[1];
    check(minim_is_string(args[0]), "syntax-error: not a string", args[0]);
    check(minim_is_null(rest) || minim_is_pair(rest), "syntax-error: not a list", rest);
    raise_message(args[0], rest);
}

// Leaves Scheme with `args[0]`, which no handler took: an error object's
// message and irritants, or else the object itself.
static minim_obj prim_raise_uncaught(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    if (minim_is_error_object(args[0]))
        raise_message(minim_error_object_message(args[0]), minim_error_object_irritants(args[0]));
    minim_error("uncaught exception", args[0]);
}

const minim_prim_def minim_error_prims[] = {
    {"%raise-uncaught", prim_raise_uncaught, 1, 1},
    {"%syntax-error", prim_syntax_error, 2, 2},
    {"%make-error-object", prim_make_error_object, 2, 2},
    {"error-object?", prim_is_error_object, 1, 1},
    {"error-object-message", prim_error_object_message, 1, 1},
    {"error-object-irritants", prim_error_object_irritants, 1, 1},
    {"read-error?", prim_is_read_error, 1, 1},
    {"file-error?", prim_is_file_error, 1, 1},
    {NULL, NULL, 0, 0},
};

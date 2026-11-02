/**
 * @file
 * Procedures and control: those the evaluator applies itself, and the
 * winders and handlers.
 */

#include "common.h"

#include <stdlib.h>

#include "gc.h"
#include "prims.h"

// Applied by the evaluator, which owns the value stack; never called.
minim_obj minim_prim_apply(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    abort();
}

// Applied by the evaluator, which returns the values; never called.
minim_obj minim_prim_values(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    abort();
}

// Applied by the evaluator, which passes the producer's values to the consumer;
// never called.
minim_obj minim_prim_call_with_values(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    abort();
}

// Applied by the evaluator, which captures the continuation; never called.
minim_obj minim_prim_call_cc(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    abort();
}

// The `dynamic-wind` calls in progress, for the prelude's `call/cc`.
static minim_obj prim_winders(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return minim_gc.winders;
}

static minim_obj prim_set_winders(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_gc.winders = args[0];
    return MINIM_VOID;
}

// The exception handlers installed, for the prelude's `raise`.
static minim_obj prim_handlers(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return minim_gc.handlers;
}

static minim_obj prim_set_handlers(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_gc.handlers = args[0];
    return MINIM_VOID;
}

// Applied by the evaluator, which evaluates the file's forms; never called.
minim_obj minim_prim_load(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    abort();
}

static minim_obj prim_is_procedure(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_procedure(args[0]));
}

const minim_prim_def minim_control_prims[] = {
    {"apply", minim_prim_apply, 2, MINIM_VARIADIC},
    {"load", minim_prim_load, 1, 1},
    {"values", minim_prim_values, 0, MINIM_VARIADIC},
    {"call-with-values", minim_prim_call_with_values, 2, 2},
    {"%call/cc", minim_prim_call_cc, 1, 1},
    {"%winders", prim_winders, 0, 0},
    {"%set-winders!", prim_set_winders, 1, 1},
    {"%handlers", prim_handlers, 0, 0},
    {"%set-handlers!", prim_set_handlers, 1, 1},
    {"procedure?", prim_is_procedure, 1, 1},
    {NULL, NULL, 0, 0},
};

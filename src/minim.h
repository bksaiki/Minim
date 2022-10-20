/*
    Public header file for Minim
*/

#ifndef _MINIM_H_
#define _MINIM_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gc/gc.h"

// Object types

typedef enum {
    /* Special types */
    MINIM_NULL_TYPE,
    MINIM_TRUE_TYPE,
    MINIM_FALSE_TYPE,
    MINIM_EOF_TYPE,
    MINIM_VOID_TYPE,

    /* Primitive types */
    MINIM_SYMBOL_TYPE,
    MINIM_FIXNUM_TYPE,
    MINIM_CHAR_TYPE,
    MINIM_STRING_TYPE,
    MINIM_PAIR_TYPE,
    MINIM_PRIM_PROC_TYPE,
    MINIM_CLOSURE_PROC_TYPE,
    MINIM_PORT_TYPE,

    /* Footer */
    MINIM_LAST_TYPE
} minim_object_type;

// Object structs

typedef struct {
    minim_object_type type;
} minim_object;

typedef struct {
    minim_object_type type;
    char *value;
} minim_symbol_object;

typedef struct {
    minim_object_type type;
    long value; /* TODO: manage size of this */
} minim_fixnum_object;

typedef struct {
    minim_object_type type;
    char *value;
} minim_string_object;

typedef struct {
    minim_object_type type;
    int value;
} minim_char_object;

typedef struct {
    minim_object_type type;
    minim_object *car;
    minim_object *cdr;
} minim_pair_object;

typedef struct {
    minim_object_type type;
    minim_object *(*fn)(minim_object *args);
} minim_prim_proc_object;

typedef struct {
    minim_object_type type;
    minim_object *args;
    minim_object *body;
    minim_object *env;
} minim_closure_proc_object;

typedef struct {
    minim_object_type type;
    FILE *stream;
    int read_only;
} minim_port_object;

// Special objects

extern minim_object *minim_null;
extern minim_object *minim_true;
extern minim_object *minim_false;
extern minim_object *minim_eof;
extern minim_object *minim_void;

// Simple Predicates

#define minim_same_type(o, t)   ((o)->type == (t))

#define minim_is_symbol(x)          (minim_same_type(x, MINIM_SYMBOL_TYPE))
#define minim_is_fixnum(x)          (minim_same_type(x, MINIM_FIXNUM_TYPE))
#define minim_is_string(x)          (minim_same_type(x, MINIM_STRING_TYPE))
#define minim_is_char(x)            (minim_same_type(x, MINIM_CHAR_TYPE))
#define minim_is_pair(x)            (minim_same_type(x, MINIM_PAIR_TYPE))
#define minim_is_prim_proc(x)       (minim_same_type(x, MINIM_PRIM_PROC_TYPE))
#define minim_is_closure_proc(x)    (minim_same_type(x, MINIM_CLOSURE_PROC_TYPE))
#define minim_is_port(x)            (minim_same_type(x, MINIM_PORT_TYPE))

#define minim_is_null(x)  ((x) == minim_null)
#define minim_is_true(x)  ((x) == minim_true)
#define minim_is_false(x) ((x) == minim_false)
#define minim_is_eof(x)   ((x) == minim_eof)
#define minim_is_void(x)  ((x) == minim_void)

// Accessors

#define minim_car(x)        (((minim_pair_object *) (x))->car)
#define minim_cdr(x)        (((minim_pair_object *) (x))->cdr)
#define minim_caar(x)       (minim_car(minim_car(x)))
#define minim_cadr(x)       (minim_car(minim_cdr(x)))
#define minim_cdar(x)       (minim_cdr(minim_car(x)))
#define minim_cddr(x)       (minim_cdr(minim_cdr(x)))

#define minim_symbol(x)         (((minim_symbol_object *) (x))->value)
#define minim_fixnum(x)         (((minim_fixnum_object *) (x))->value)
#define minim_string(x)         (((minim_string_object *) (x))->value)
#define minim_char(x)           (((minim_char_object *) (x))->value)
#define minim_prim_proc(x)      (((minim_prim_proc_object *) (x))->fn)
#define minim_closure_args(x)   (((minim_closure_proc_object *) (x))->args)
#define minim_closure_body(x)   (((minim_closure_proc_object *) (x))->body)
#define minim_closure_env(x)    (((minim_closure_proc_object *) (x))->env)

#define minim_port_is_ro(x)     (((minim_port_object *) (x))->read_only != 0)
#define minim_port(x)           (((minim_port_object *) (x))->stream)

// Complex predicates

#define minim_is_input_port(x)      (minim_is_port(x) && minim_port_is_ro(x))
#define minim_is_output_port(x)     (minim_is_port(x) && !minim_port_is_ro(x))

// Typedefs

typedef minim_object *(*minim_prim_proc_t)(minim_object *);

// Constructors

minim_object *make_char(int c);
minim_object *make_fixnum(long v);
minim_object *make_symbol(const char *s);
minim_object *make_string(const char *s);
minim_object *make_pair(minim_object *car, minim_object *cdr);
minim_object *make_prim_proc(minim_prim_proc_t proc);

// Primitives

int minim_is_eq(minim_object *a, minim_object *b);
int minim_is_equal(minim_object *a, minim_object *b);

#endif  // _MINIM_H_

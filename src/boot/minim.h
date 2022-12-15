/*
    Public header file for Minim
*/

#ifndef _MINIM_H_
#define _MINIM_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../gc/gc.h"

// Limits

#define ARG_MAX     32767
#define VALUES_MAX  32767

// Arity

typedef enum {
    PROC_ARITY_FIXED,
    PROC_ARITY_UNBOUNDED,
    PROC_ARITY_RANGE
} proc_arity_type;

typedef struct proc_arity {
    proc_arity_type type;
    short arity_min, arity_max;
} proc_arity;

#define proc_arity_same_type(p, t)      ((p)->type == (t))
#define proc_arity_is_fixed(p)          (proc_arity_same_type(p, PROC_ARITY_FIXED))
#define proc_arity_is_unbounded(p)      (proc_arity_same_type(p, PROC_ARITY_UNBOUNDED))
#define proc_arity_is_range(p)          (proc_arity_same_type(p, PROC_ARITY_RANGE))

#define proc_arity_min(p)               ((p)->arity_min)
#define proc_arity_max(p)               ((p)->arity_max)

#define proc_arity_is_between(p, min, max)      \
    (((min) <= (p)->arity_min) && ((p)->arity_max <= (max)))

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
    MINIM_SYNTAX_TYPE,

    /* Composite types */
    MINIM_VALUES_TYPE,

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
    struct proc_arity arity;
    minim_object *(*fn)(minim_object *args);
    char *name;
} minim_prim_proc_object;

typedef struct {
    minim_object_type type;
    struct proc_arity arity;
    minim_object *args;
    minim_object *body;
    minim_object *env;
    char *name;
} minim_closure_proc_object;

typedef struct {
    minim_object_type type;
    FILE *stream;
    int flags;
} minim_port_object;

typedef struct {
    minim_object_type type;
    minim_object *e;
    minim_object *loc;
} minim_syntax_object;

// Special objects

extern minim_object *minim_null;
extern minim_object *minim_true;
extern minim_object *minim_false;
extern minim_object *minim_eof;
extern minim_object *minim_void;
extern minim_object *minim_values;

extern minim_object **values_buffer;
extern int values_buffer_size;
extern int values_buffer_count;

#define values_buffer_ref(idx)    (values_buffer[idx])

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
#define minim_is_syntax(x)          (minim_same_type(x, MINIM_SYNTAX_TYPE))
#define minim_is_values(x)          (minim_same_type(x, MINIM_VALUES_TYPE))

#define minim_is_null(x)    ((x) == minim_null)
#define minim_is_true(x)    ((x) == minim_true)
#define minim_is_false(x)   ((x) == minim_false)
#define minim_is_eof(x)     ((x) == minim_eof)
#define minim_is_void(x)    ((x) == minim_void)

// Flags

#define MINIM_PORT_READ_ONLY        0x1
#define MINIM_PORT_OPEN             0x2

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

#define minim_prim_proc_name(x) (((minim_prim_proc_object *) (x))->name)
#define minim_prim_arity(x)     (((minim_prim_proc_object *) (x))->arity)
#define minim_prim_proc(x)      (((minim_prim_proc_object *) (x))->fn)

#define minim_closure_args(x)   (((minim_closure_proc_object *) (x))->args)
#define minim_closure_body(x)   (((minim_closure_proc_object *) (x))->body)
#define minim_closure_env(x)    (((minim_closure_proc_object *) (x))->env)
#define minim_closure_name(x)   (((minim_closure_proc_object *) (x))->name)
#define minim_closure_arity(x)  (((minim_closure_proc_object *) (x))->arity)

#define minim_port_is_ro(x)     (((minim_port_object *) (x))->flags & MINIM_PORT_READ_ONLY)
#define minim_port_is_open(x)   (((minim_port_object *) (x))->flags & MINIM_PORT_OPEN)
#define minim_port(x)           (((minim_port_object *) (x))->stream)

#define minim_syntax_e(x)       (((minim_syntax_object *) (x))->e)
#define minim_syntax_loc(x)     (((minim_syntax_object *) (x))->loc)

// Setters

#define minim_port_set(x, f)        (((minim_port_object *) (x))->flags |= (f))
#define minim_port_unset(x, f)      (((minim_port_object *) (x))->flags &= ~(f))

// Complex predicates

#define minim_is_proc(x)            (minim_is_prim_proc(x) || minim_is_closure_proc(x))
#define minim_is_input_port(x)      (minim_is_port(x) && minim_port_is_ro(x))
#define minim_is_output_port(x)     (minim_is_port(x) && !minim_port_is_ro(x))

// Typedefs

typedef minim_object *(*minim_prim_proc_t)(minim_object *);

// Constructors

minim_object *make_char(int c);
minim_object *make_fixnum(long v);
minim_object *make_symbol(const char *s);
minim_object *make_string(const char *s);
minim_object *make_string2(char *s);
minim_object *make_pair(minim_object *car, minim_object *cdr);
minim_object *make_prim_proc(minim_prim_proc_t proc, char *name, short min_arity, short max_arity);
minim_object *make_closure_proc(minim_object *args, minim_object *body, minim_object *env, short min_arity, short max_arity);
minim_object *make_input_port(FILE *stream);
minim_object *make_output_port(FILE *stream);
minim_object *make_syntax(minim_object *e, minim_object *loc);

// Primitives

int minim_is_eq(minim_object *a, minim_object *b);
int minim_is_equal(minim_object *a, minim_object *b);

#endif  // _MINIM_H_

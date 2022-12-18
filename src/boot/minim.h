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

// Constants

#define MINIM_VERSION      "0.4.0"

#define ARG_MAX                     32767
#define VALUES_MAX                  32767
#define SYMBOL_MAX_LEN              4096

#define INIT_VALUES_BUFFER_LEN      10


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

extern minim_object *quote_symbol;
extern minim_object *define_symbol;
extern minim_object *define_values_symbol;
extern minim_object *let_symbol;
extern minim_object *let_values_symbol;
extern minim_object *letrec_symbol;
extern minim_object *letrec_values_symbol;
extern minim_object *setb_symbol;
extern minim_object *if_symbol;
extern minim_object *lambda_symbol;
extern minim_object *begin_symbol;
extern minim_object *cond_symbol;
extern minim_object *else_symbol;
extern minim_object *and_symbol;
extern minim_object *or_symbol;
extern minim_object *syntax_symbol;
extern minim_object *syntax_loc_symbol;
extern minim_object *quote_syntax_symbol;

// Simple predicates

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

#define minim_is_bool(x)            (minim_is_true(x) || minim_is_false(x))
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

// Object operations

int minim_is_eq(minim_object *a, minim_object *b);
int minim_is_equal(minim_object *a, minim_object *b);

minim_object *call_with_args(minim_object *proc,
                             minim_object *args,
                             minim_object *env);

minim_object *call_with_values(minim_object *producer,
                               minim_object *consumer,
                               minim_object *env);

int is_list(minim_object *x);
long list_length(minim_object *xs);
minim_object *make_assoc(minim_object *xs, minim_object *ys);
minim_object *copy_list(minim_object *xs);
minim_object *andmap(minim_object *proc, minim_object *lst, minim_object *env);
minim_object *ormap(minim_object *proc, minim_object *lst, minim_object *env);

minim_object *strip_syntax(minim_object *o);
minim_object *to_syntax(minim_object *o);

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);
void write_object2(FILE *out, minim_object *o, int quote, int display);

// Interpreter

minim_object *eval_expr(minim_object *expr, minim_object *env);

// Environments

minim_object *setup_env();
minim_object *make_env();
minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env);

void env_define_var_no_check(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_lookup_var(minim_object *env, minim_object *var);
int env_var_is_defined(minim_object *env, minim_object *var, int recursive);

extern minim_object *empty_env;

// Symbols

typedef struct intern_table_bucket {
    minim_object *sym;
    struct intern_table_bucket *next;
} intern_table_bucket;

typedef struct intern_table {
    intern_table_bucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} intern_table;

intern_table *make_intern_table();
minim_object *intern_symbol(intern_table *itab, const char *sym);

// Threads

typedef struct minim_thread {
    minim_object *env;

    minim_object *input_port;
    minim_object *output_port;
    minim_object *current_directory;
    minim_object *boot_expander;

    minim_object **values_buffer;
    int values_buffer_size;
    int values_buffer_count;

    int pid;
} minim_thread;

#define global_env(th)                  ((th)->env)
#define input_port(th)                  ((th)->input_port)
#define output_port(th)                 ((th)->output_port)
#define current_directory(th)           ((th)->current_directory)
#define boot_expander(th)               ((th)->boot_expander)

#define values_buffer(th)               ((th)->values_buffer)
#define values_buffer_ref(th, idx)      ((th)->values_buffer[(idx)])
#define values_buffer_size(th)          ((th)->values_buffer_size)
#define values_buffer_count(th)         ((th)->values_buffer_count)

void resize_values_buffer(minim_thread *th, int size);

// Globals

typedef struct minim_globals {
    minim_thread *current_thread;
    intern_table *symbols;
} minim_globals;

extern minim_globals *globals;

#define current_thread()    (globals->current_thread)
#define intern(s)           (intern_symbol(globals->symbols, s))

// System

char *get_file_dir(const char *realpath);
char* get_current_dir();
void set_current_dir(const char *str);

minim_object *load_file(const char *fname);

// Exceptions

void bad_syntax_exn(minim_object *expr);
void bad_type_exn(const char *name, const char *type, minim_object *x);

// Primitives

#define DEFINE_PRIM_PROC(name) \
    minim_object *name ## _proc(minim_object *);

// special objects
DEFINE_PRIM_PROC(is_null);
DEFINE_PRIM_PROC(is_void);
DEFINE_PRIM_PROC(is_eof);
DEFINE_PRIM_PROC(is_bool);
DEFINE_PRIM_PROC(not);
DEFINE_PRIM_PROC(void);
// equality
DEFINE_PRIM_PROC(eq);
DEFINE_PRIM_PROC(equal);
// procedures
DEFINE_PRIM_PROC(is_procedure);
DEFINE_PRIM_PROC(call_with_values);
DEFINE_PRIM_PROC(values);
DEFINE_PRIM_PROC(apply)
DEFINE_PRIM_PROC(eval);
// pairs
DEFINE_PRIM_PROC(is_pair);
DEFINE_PRIM_PROC(cons);
DEFINE_PRIM_PROC(car);
DEFINE_PRIM_PROC(cdr);
DEFINE_PRIM_PROC(set_car);
DEFINE_PRIM_PROC(set_cdr);
// lists
DEFINE_PRIM_PROC(is_list);
DEFINE_PRIM_PROC(list);
DEFINE_PRIM_PROC(length);
DEFINE_PRIM_PROC(reverse);
DEFINE_PRIM_PROC(append);
DEFINE_PRIM_PROC(andmap);
DEFINE_PRIM_PROC(ormap);
// numbers
DEFINE_PRIM_PROC(is_fixnum);
DEFINE_PRIM_PROC(add);
DEFINE_PRIM_PROC(sub);
DEFINE_PRIM_PROC(mul);
DEFINE_PRIM_PROC(div);
DEFINE_PRIM_PROC(remainder);
DEFINE_PRIM_PROC(modulo);
DEFINE_PRIM_PROC(number_eq);
DEFINE_PRIM_PROC(number_ge);
DEFINE_PRIM_PROC(number_le);
DEFINE_PRIM_PROC(number_gt);
DEFINE_PRIM_PROC(number_lt);
// symbol
DEFINE_PRIM_PROC(is_symbol);
// characters
DEFINE_PRIM_PROC(is_char);
DEFINE_PRIM_PROC(char_to_integer);
DEFINE_PRIM_PROC(integer_to_char);
// strings
DEFINE_PRIM_PROC(is_string);
DEFINE_PRIM_PROC(make_string);
DEFINE_PRIM_PROC(string);
DEFINE_PRIM_PROC(string_length);
DEFINE_PRIM_PROC(string_ref);
DEFINE_PRIM_PROC(string_set);
DEFINE_PRIM_PROC(number_to_string);
DEFINE_PRIM_PROC(string_to_number);
DEFINE_PRIM_PROC(symbol_to_string);
DEFINE_PRIM_PROC(string_to_symbol);
// environment
DEFINE_PRIM_PROC(empty_environment);
DEFINE_PRIM_PROC(extend_environment);
DEFINE_PRIM_PROC(environment_variable_value);
DEFINE_PRIM_PROC(environment_set_variable_value);
DEFINE_PRIM_PROC(environment);
DEFINE_PRIM_PROC(current_environment);
DEFINE_PRIM_PROC(interaction_environment);
// syntax
DEFINE_PRIM_PROC(is_syntax);
DEFINE_PRIM_PROC(syntax_e);
DEFINE_PRIM_PROC(syntax_loc);
DEFINE_PRIM_PROC(to_syntax);
DEFINE_PRIM_PROC(syntax_error);
// I/O
DEFINE_PRIM_PROC(is_input_port);
DEFINE_PRIM_PROC(is_output_port);
DEFINE_PRIM_PROC(current_input_port);
DEFINE_PRIM_PROC(current_output_port);
DEFINE_PRIM_PROC(open_input_port);
DEFINE_PRIM_PROC(open_output_port);
DEFINE_PRIM_PROC(close_input_port);
DEFINE_PRIM_PROC(close_output_port);
DEFINE_PRIM_PROC(read);
DEFINE_PRIM_PROC(read_char);
DEFINE_PRIM_PROC(peek_char);
DEFINE_PRIM_PROC(char_is_ready);
DEFINE_PRIM_PROC(display);
DEFINE_PRIM_PROC(write);
DEFINE_PRIM_PROC(write_char);
DEFINE_PRIM_PROC(newline);
// System
DEFINE_PRIM_PROC(load);
DEFINE_PRIM_PROC(error);
DEFINE_PRIM_PROC(current_directory);
DEFINE_PRIM_PROC(exit);

#endif  // _MINIM_H_

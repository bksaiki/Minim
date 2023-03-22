/*
    Public header file for Minim
*/

#ifndef _MINIM_H_
#define _MINIM_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../gc/gc.h"

// Config

#if defined (__GNUC__)
#define MINIM_GCC     1
#define NORETURN    __attribute__ ((noreturn))
#else
#error "compiler not supported"
#endif

// Constants

#define MINIM_VERSION      "0.3.5"

#define ARG_MAX                     32767
#define VALUES_MAX                  32767
#define RECORD_FIELD_MAX            32767
#define SYMBOL_MAX_LEN              4096

#define INIT_VALUES_BUFFER_LEN      10
#define ENVIRONMENT_VECTOR_MAX      10

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
    MINIM_VECTOR_TYPE,
    MINIM_PRIM_PROC_TYPE,
    MINIM_CLOSURE_PROC_TYPE,
    MINIM_PORT_TYPE,

    /* Composite types */
    MINIM_RECORD_TYPE,
    MINIM_BOX_TYPE,
    MINIM_HASHTABLE_TYPE,
    MINIM_SYNTAX_TYPE,

    /* Transform types */
    MINIM_PATTERN_VAR_TYPE,

    /* Runtime types */
    MINIM_ENVIRONMENT_TYPE,
    MINIM_VALUES_TYPE,

    /* Compiled types */
    MINIM_NATIVE_CLOSURE_TYPE,

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
    minim_object **arr;
    long len;
} minim_vector_object;

typedef struct {
    minim_object_type type;
    struct proc_arity arity;
    minim_object *(*fn)(int, minim_object **);
    char *name;
} minim_prim_proc_object;

typedef struct {
    minim_object_type type;
    struct proc_arity arity;
    minim_object *args;
    minim_object *body;
    minim_object *env;
    char *name;
} minim_closure_object;

typedef struct {
    minim_object_type type;
    struct proc_arity arity;
    minim_object *env;
    void *fn;
    char *name;
} minim_native_closure_object;

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

typedef struct {
    minim_object_type type;
    minim_object *rtd;
    minim_object **fields;
    int fieldc;
} minim_record_object;

typedef struct {
    minim_object_type type;
    minim_object *o;
} minim_box_object;

typedef struct {
    minim_object_type type;
    minim_object *hash, *equiv;
    minim_object **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} minim_hashtable_object;

typedef struct {
    minim_object_type type;
    minim_object *value;
    minim_object *depth;
} minim_pattern_var_object;

typedef struct {
    minim_object_type type;
    minim_object *prev;
    minim_object *bindings;
} minim_env;

// Special objects

extern minim_object *minim_null;
extern minim_object *minim_empty_vec;
extern minim_object *minim_true;
extern minim_object *minim_false;
extern minim_object *minim_eof;
extern minim_object *minim_void;
extern minim_object *minim_values;
extern minim_object *minim_base_rtd;

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
#define minim_is_vector(x)          (minim_same_type(x, MINIM_VECTOR_TYPE))
#define minim_is_prim_proc(x)       (minim_same_type(x, MINIM_PRIM_PROC_TYPE))
#define minim_is_closure(x)         (minim_same_type(x, MINIM_CLOSURE_PROC_TYPE))
#define minim_is_port(x)            (minim_same_type(x, MINIM_PORT_TYPE))
#define minim_is_record(x)          (minim_same_type(x, MINIM_RECORD_TYPE))
#define minim_is_box(x)             (minim_same_type(x, MINIM_BOX_TYPE))
#define minim_is_hashtable(x)       (minim_same_type(x, MINIM_HASHTABLE_TYPE))
#define minim_is_syntax(x)          (minim_same_type(x, MINIM_SYNTAX_TYPE))
#define minim_is_pattern_var(x)     (minim_same_type(x, MINIM_PATTERN_VAR_TYPE))
#define minim_is_env(x)             (minim_same_type(x, MINIM_ENVIRONMENT_TYPE))
#define minim_is_values(x)          (minim_same_type(x, MINIM_VALUES_TYPE))
#define minim_is_native_closure(x)  (minim_same_type(x, MINIM_NATIVE_CLOSURE_TYPE))

#define minim_is_null(x)        ((x) == minim_null)
#define minim_is_empty_vec(x)   ((x) == minim_empty_vec)
#define minim_is_true(x)        ((x) == minim_true)
#define minim_is_false(x)       ((x) == minim_false)
#define minim_is_eof(x)         ((x) == minim_eof)
#define minim_is_void(x)        ((x) == minim_void)
#define minim_is_base_rtd(x)    ((x) == minim_base_rtd)

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

#define minim_vector_len(x)     (((minim_vector_object *) (x))->len)
#define minim_vector_arr(x)     (((minim_vector_object *) (x))->arr)
#define minim_vector_ref(x, i)  (((minim_vector_object *) (x))->arr[i])

#define minim_symbol(x)         (((minim_symbol_object *) (x))->value)
#define minim_fixnum(x)         (((minim_fixnum_object *) (x))->value)
#define minim_string(x)         (((minim_string_object *) (x))->value)
#define minim_char(x)           (((minim_char_object *) (x))->value)

#define minim_prim_proc_name(x) (((minim_prim_proc_object *) (x))->name)
#define minim_prim_arity(x)     (((minim_prim_proc_object *) (x))->arity)
#define minim_prim_proc(x)      (((minim_prim_proc_object *) (x))->fn)

#define minim_closure_args(x)   (((minim_closure_object *) (x))->args)
#define minim_closure_body(x)   (((minim_closure_object *) (x))->body)
#define minim_closure_env(x)    (((minim_closure_object *) (x))->env)
#define minim_closure_name(x)   (((minim_closure_object *) (x))->name)
#define minim_closure_arity(x)  (((minim_closure_object *) (x))->arity)

#define minim_port_is_ro(x)     (((minim_port_object *) (x))->flags & MINIM_PORT_READ_ONLY)
#define minim_port_is_open(x)   (((minim_port_object *) (x))->flags & MINIM_PORT_OPEN)
#define minim_port(x)           (((minim_port_object *) (x))->stream)

#define minim_syntax_e(x)       (((minim_syntax_object *) (x))->e)
#define minim_syntax_loc(x)     (((minim_syntax_object *) (x))->loc)

#define minim_record_rtd(x)         (((minim_record_object *) (x))->rtd)
#define minim_record_ref(x, i)      (((minim_record_object *) (x))->fields[i])
#define minim_record_count(x)       (((minim_record_object *) (x))->fieldc)

#define minim_box_contents(x)           (((minim_box_object *) (x))->o)

#define minim_hashtable_buckets(x)      (((minim_hashtable_object *) (x))->buckets)
#define minim_hashtable_bucket(x, i)    ((((minim_hashtable_object *) (x))->buckets)[i])
#define minim_hashtable_alloc_ptr(x)    (((minim_hashtable_object *) (x))->alloc_ptr)
#define minim_hashtable_alloc(x)        (((minim_hashtable_object *) (x))->alloc)
#define minim_hashtable_size(x)         (((minim_hashtable_object *) (x))->size)
#define minim_hashtable_hash(x)         (((minim_hashtable_object *) (x))->hash)
#define minim_hashtable_equiv(x)        (((minim_hashtable_object *) (x))->equiv)

#define minim_pattern_var_value(x)      (((minim_pattern_var_object *) (x))->value)
#define minim_pattern_var_depth(x)      (((minim_pattern_var_object *) (x))->depth)

#define minim_env_bindings(x)           (((minim_env *) (x))->bindings)
#define minim_env_prev(x)               (((minim_env *) (x))->prev)

#define minim_native_closure_arity(x)   (((minim_native_closure_object *) (x))->arity)
#define minim_native_closure_name(x)    (((minim_native_closure_object *) (x))->name)
#define minim_native_closure_code(x)    (((minim_native_closure_object *) (x))->code)
#define minim_native_closure_env(x)     (((minim_native_closure_object *) (x))->env)

// Setters

#define minim_port_set(x, f)        (((minim_port_object *) (x))->flags |= (f))
#define minim_port_unset(x, f)      (((minim_port_object *) (x))->flags &= ~(f))

// Complex predicates

#define minim_is_bool(x)            (minim_is_true(x) || minim_is_false(x))
#define minim_is_proc(x)            (minim_is_prim_proc(x) || minim_is_closure(x))
#define minim_is_input_port(x)      (minim_is_port(x) && minim_port_is_ro(x))
#define minim_is_output_port(x)     (minim_is_port(x) && !minim_port_is_ro(x))

// Typedefs

typedef minim_object *(*minim_prim_proc_t)(int argc, minim_object **);

// Constructors

minim_object *make_char(int c);
minim_object *make_fixnum(long v);
minim_object *make_symbol(const char *s);
minim_object *make_string(const char *s);
minim_object *make_string2(char *s);
minim_object *make_pair(minim_object *car, minim_object *cdr);
minim_object *make_vector(long len, minim_object *init);
minim_object *make_record(minim_object *rtd, int fieldc);
minim_object *make_box(minim_object *x);
minim_object *make_hashtable(minim_object *hash_fn, minim_object *equiv_fn);
minim_object *make_hashtable2(minim_object *hash_fn, minim_object *equiv_fn, size_t size_hint);
minim_object *make_prim_proc(minim_prim_proc_t proc, char *name, short min_arity, short max_arity);
minim_object *make_closure(minim_object *args, minim_object *body, minim_object *env, short min_arity, short max_arity);
minim_object *make_native_closure(minim_object *env, void *fn, short min_arity, short max_arity);
minim_object *make_input_port(FILE *stream);
minim_object *make_output_port(FILE *stream);
minim_object *make_pattern_var(minim_object *value, minim_object *depth);
minim_object *make_syntax(minim_object *e, minim_object *loc);

// Object operations

int minim_is_eq(minim_object *a, minim_object *b);
int minim_is_equal(minim_object *a, minim_object *b);

minim_object *call_with_args(minim_object *proc, minim_object *env);
minim_object *call_with_values(minim_object *producer, minim_object *consumer, minim_object *env);

int is_list(minim_object *x);
long list_length(minim_object *xs);
minim_object *make_assoc(minim_object *xs, minim_object *ys);
minim_object *copy_list(minim_object *xs);

minim_object *for_each(minim_object *proc, int argc, minim_object **args, minim_object *env);
minim_object *map_list(minim_object *proc, int argc, minim_object **args, minim_object *env);
minim_object *andmap(minim_object *proc, int argc, minim_object **args, minim_object *env);
minim_object *ormap(minim_object *proc, int argc, minim_object **args, minim_object *env);

minim_object *strip_syntax(minim_object *o);
minim_object *to_syntax(minim_object *o);

// I/O

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);
void write_object2(FILE *out, minim_object *o, int quote, int display);
void minim_fprintf(FILE *o, const char *form, int v_count, minim_object **vs, const char *prim_name);

// Interpreter

#define CALL_ARGS_DEFAULT       10
#define SAVED_ARGS_DEFAULT      10

minim_object *eval_expr(minim_object *expr, minim_object *env);

typedef struct {
    minim_object **call_args, **saved_args;
    long call_args_count, saved_args_count;
    long call_args_size, saved_args_size;
} interp_rt;

void push_saved_arg(minim_object *arg);
void push_call_arg(minim_object *arg);
void prepare_call_args(long count);
void clear_call_args();

void assert_no_call_args();

// Environments

minim_object *make_environment(minim_object *prev);
minim_object *setup_env();
minim_object *make_env();

void env_define_var_no_check(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val);
minim_object *env_lookup_var(minim_object *env, minim_object *var);
int env_var_is_defined(minim_object *env, minim_object *var, int recursive);

extern minim_object *empty_env;

// Memory

void check_native_closure_arity(minim_object *fn, short argc);
minim_object *call_compiled(minim_object *env, minim_object *addr);
minim_object *make_rest_argument(minim_object *args[], short argc);

// Records

/*
    Records come in two flavors:
     - record type descriptor
     - record value

    Record type descriptors have the following structure:
    
    +--------------------------+
    |  RTD pointer (base RTD)  | (rtd)
    +--------------------------+
    |           Name           | (fields[0])
    +--------------------------+
    |          Parent          | (fields[1])
    +--------------------------+
    |            UID           |  ...
    +--------------------------+
    |          Opaque?         |
    +--------------------------+
    |          Sealed?         |
    +--------------------------+
    |  Protocol (constructor)  |
    +--------------------------+
    |    Field 0 Descriptor    |
    +--------------------------+
    |    Field 1 Descriptor    |
    +--------------------------+
    |                          |
                ...
    |                          |
    +--------------------------+
    |    Field N Descriptor    |
    +--------------------------+

    Field descriptors have the following possible forms:
    
    '(immutable <name> <accessor-name>)
    '(mutable <name> <accessor-name> <mutator-name>)
    '(immutable <name>)
    '(mutable <name>)
    <name>

    The third and forth forms are just shorthand for
    
    '(immutable <name> <rtd-name>-<name>)
    '(mutable <name> <rtd-name>-<name> <rtd-name>-<name>-set!)

    respectively. The fifth form is just an abbreviation for

    '(immutable <name>)

    There is always a unique record type descriptor during runtime:
    the base record type descriptor. It cannot be accessed during runtime
    and serves as the "record type descriptor" of all record type descriptors.

    Record values have the following structure:

    +--------------------------+
    |        RTD pointer       |
    +--------------------------+
    |          Field 0         |
    +--------------------------+
    |          Field 1         |
    +--------------------------+
    |                          |
                ...
    |                          |
    +--------------------------+
    |          Field N         |
    +--------------------------+
*/

#define record_rtd_min_size         6

#define record_rtd_name(rtd)        minim_record_ref(rtd, 0)
#define record_rtd_parent(rtd)      minim_record_ref(rtd, 1)
#define record_rtd_uid(rtd)         minim_record_ref(rtd, 2)
#define record_rtd_opaque(rtd)      minim_record_ref(rtd, 3)
#define record_rtd_sealed(rtd)      minim_record_ref(rtd, 4)
#define record_rtd_protocol(rtd)    minim_record_ref(rtd, 5)
#define record_rtd_field(rtd, i)    minim_record_ref(rtd, (record_rtd_min_size + (i)))

#define record_is_opaque(o)     (record_rtd_opaque(minim_record_rtd(o)) == minim_true)
#define record_is_sealed(o)     (record_rtd_sealed(minim_record_rtd(o)) == minim_true)

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

// Hashtables

int hashtable_set(minim_object *ht, minim_object *k, minim_object *v);
minim_object *hashtable_find(minim_object *ht, minim_object *k);
minim_object *hashtable_keys(minim_object *ht);

// Threads

typedef struct minim_thread {
    minim_object *env;

    minim_object *input_port;
    minim_object *output_port;
    minim_object *current_directory;
    minim_object *boot_expander;
    minim_object *command_line;

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
#define command_line(th)                ((th)->command_line)

#define values_buffer(th)               ((th)->values_buffer)
#define values_buffer_ref(th, idx)      ((th)->values_buffer[(idx)])
#define values_buffer_size(th)          ((th)->values_buffer_size)
#define values_buffer_count(th)         ((th)->values_buffer_count)

void resize_values_buffer(minim_thread *th, int size);

// Globals

typedef struct minim_globals {
    interp_rt irt;
    minim_thread *current_thread;
    intern_table *symbols;
} minim_globals;

extern minim_globals *globals;
extern size_t bucket_sizes[];

#define current_thread()    (globals->current_thread)
#define intern(s)           (intern_symbol(globals->symbols, s))

#define irt_call_args               (globals->irt.call_args)
#define irt_saved_args              (globals->irt.saved_args)
#define irt_call_args_count         (globals->irt.call_args_count)
#define irt_saved_args_count        (globals->irt.saved_args_count)
#define irt_call_args_size          (globals->irt.call_args_size)
#define irt_saved_args_size         (globals->irt.saved_args_size)

// System

char *get_file_dir(const char *realpath);
char* get_current_dir();
void set_current_dir(const char *str);

void *alloc_page(size_t size);
int make_page_executable(void *page, size_t size);
int make_page_write_only(void *page, size_t size);

minim_object *load_file(const char *fname);

NORETURN void minim_shutdown(int code);

// Exceptions

NORETURN void arity_mismatch_exn(const char *name, proc_arity *arity, short actual);
NORETURN void bad_syntax_exn(minim_object *expr);
NORETURN void bad_type_exn(const char *name, const char *type, minim_object *x);
NORETURN void result_arity_exn(short expected, short actual);
NORETURN void uncallable_prim_exn(const char *name);

// Primitives

#define DEFINE_PRIM_PROC(name) \
    minim_object *name ## _proc(int, minim_object **);

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
DEFINE_PRIM_PROC(caar);
DEFINE_PRIM_PROC(cadr);
DEFINE_PRIM_PROC(cdar);
DEFINE_PRIM_PROC(cddr);
DEFINE_PRIM_PROC(caaar);
DEFINE_PRIM_PROC(caadr);
DEFINE_PRIM_PROC(cadar);
DEFINE_PRIM_PROC(caddr);
DEFINE_PRIM_PROC(cdaar);
DEFINE_PRIM_PROC(cdadr);
DEFINE_PRIM_PROC(cddar);
DEFINE_PRIM_PROC(cdddr);
DEFINE_PRIM_PROC(caaaar);
DEFINE_PRIM_PROC(caaadr);
DEFINE_PRIM_PROC(caadar);
DEFINE_PRIM_PROC(caaddr);
DEFINE_PRIM_PROC(cadaar);
DEFINE_PRIM_PROC(cadadr);
DEFINE_PRIM_PROC(caddar);
DEFINE_PRIM_PROC(cadddr);
DEFINE_PRIM_PROC(cdaaar);
DEFINE_PRIM_PROC(cdaadr);
DEFINE_PRIM_PROC(cdadar);
DEFINE_PRIM_PROC(cdaddr);
DEFINE_PRIM_PROC(cddaar);
DEFINE_PRIM_PROC(cddadr);
DEFINE_PRIM_PROC(cdddar);
DEFINE_PRIM_PROC(cddddr);
DEFINE_PRIM_PROC(set_car);
DEFINE_PRIM_PROC(set_cdr);
// lists
DEFINE_PRIM_PROC(is_list);
DEFINE_PRIM_PROC(list);
DEFINE_PRIM_PROC(make_list);
DEFINE_PRIM_PROC(length);
DEFINE_PRIM_PROC(reverse);
DEFINE_PRIM_PROC(append);
DEFINE_PRIM_PROC(for_each);
DEFINE_PRIM_PROC(map);
DEFINE_PRIM_PROC(andmap);
DEFINE_PRIM_PROC(ormap);
// vectors
DEFINE_PRIM_PROC(is_vector);
DEFINE_PRIM_PROC(make_vector);
DEFINE_PRIM_PROC(vector);
DEFINE_PRIM_PROC(vector_length);
DEFINE_PRIM_PROC(vector_ref);
DEFINE_PRIM_PROC(vector_set);
DEFINE_PRIM_PROC(vector_fill);
DEFINE_PRIM_PROC(vector_to_list);
DEFINE_PRIM_PROC(list_to_vector);
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
DEFINE_PRIM_PROC(string_append);
DEFINE_PRIM_PROC(number_to_string);
DEFINE_PRIM_PROC(string_to_number);
DEFINE_PRIM_PROC(symbol_to_string);
DEFINE_PRIM_PROC(string_to_symbol);
DEFINE_PRIM_PROC(format);
// record
DEFINE_PRIM_PROC(is_record);
DEFINE_PRIM_PROC(is_record_rtd);
DEFINE_PRIM_PROC(make_rtd);
DEFINE_PRIM_PROC(record_rtd);
DEFINE_PRIM_PROC(record_type_name);
DEFINE_PRIM_PROC(record_type_parent);
DEFINE_PRIM_PROC(record_type_uid);
DEFINE_PRIM_PROC(record_type_sealed);
DEFINE_PRIM_PROC(record_type_opaque);
DEFINE_PRIM_PROC(record_type_fields);
DEFINE_PRIM_PROC(record_type_field_mutable);
// boxes
DEFINE_PRIM_PROC(is_box);
DEFINE_PRIM_PROC(box);
DEFINE_PRIM_PROC(unbox);
DEFINE_PRIM_PROC(box_set);
// hashtable
DEFINE_PRIM_PROC(is_hashtable);
DEFINE_PRIM_PROC(make_eq_hashtable);
DEFINE_PRIM_PROC(make_hashtable);
DEFINE_PRIM_PROC(hashtable_size);
DEFINE_PRIM_PROC(hashtable_contains);
DEFINE_PRIM_PROC(hashtable_set);
DEFINE_PRIM_PROC(hashtable_delete);
DEFINE_PRIM_PROC(hashtable_update);
DEFINE_PRIM_PROC(hashtable_ref);
DEFINE_PRIM_PROC(hashtable_keys);
DEFINE_PRIM_PROC(hashtable_copy);
DEFINE_PRIM_PROC(hashtable_clear);
// hash functions
DEFINE_PRIM_PROC(eq_hash);
DEFINE_PRIM_PROC(equal_hash);
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
DEFINE_PRIM_PROC(to_datum);
DEFINE_PRIM_PROC(syntax_error);
DEFINE_PRIM_PROC(syntax_to_list);
// pattern variables
DEFINE_PRIM_PROC(is_pattern_var);
DEFINE_PRIM_PROC(make_pattern_var);
DEFINE_PRIM_PROC(pattern_var_value);
DEFINE_PRIM_PROC(pattern_var_depth);
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
DEFINE_PRIM_PROC(fprintf);
DEFINE_PRIM_PROC(printf);
// System
DEFINE_PRIM_PROC(load);
DEFINE_PRIM_PROC(error);
DEFINE_PRIM_PROC(current_directory);
DEFINE_PRIM_PROC(exit);
DEFINE_PRIM_PROC(command_line);
// Memory
DEFINE_PRIM_PROC(install_literal_bundle);
DEFINE_PRIM_PROC(install_proc_bundle);
DEFINE_PRIM_PROC(reinstall_proc_bundle);
DEFINE_PRIM_PROC(runtime_address);
DEFINE_PRIM_PROC(enter_compiled);

#endif  // _MINIM_H_

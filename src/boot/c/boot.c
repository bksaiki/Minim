/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "boot.h"

//
//  Special objects
//

minim_object *minim_null;
minim_object *minim_true;
minim_object *minim_false;
minim_object *minim_eof;
minim_object *minim_void;
minim_object *minim_values;

minim_object *quote_symbol;
minim_object *define_symbol;
minim_object *define_values_symbol;
minim_object *let_symbol;
minim_object *let_values_symbol;
minim_object *letrec_symbol;
minim_object *letrec_values_symbol;
minim_object *setb_symbol;
minim_object *if_symbol;
minim_object *lambda_symbol;
minim_object *begin_symbol;
minim_object *cond_symbol;
minim_object *else_symbol;
minim_object *and_symbol;
minim_object *or_symbol;
minim_object *syntax_symbol;
minim_object *syntax_loc_symbol;
minim_object *quote_syntax_symbol;

minim_globals *globals;
minim_object *empty_env;

//
//  forward declarations
//

long list_length(minim_object *xs);
static minim_object *call_with_args(minim_object *proc, minim_object *args, minim_object *env);

//
//  Constructors
//

minim_object *make_char(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
    return ((minim_object *) o);
}

minim_object *make_fixnum(long v) {
    minim_fixnum_object *o = GC_alloc(sizeof(minim_fixnum_object));
    o->type = MINIM_FIXNUM_TYPE;
    o->value = v;
    return ((minim_object *) o);
}

minim_object *make_symbol(const char *s) {
    minim_symbol_object *o = GC_alloc(sizeof(minim_symbol_object));
    int len = strlen(s);
    
    o->type = MINIM_SYMBOL_TYPE;
    o->value = GC_alloc_atomic((len + 1) * sizeof(char));
    strncpy(o->value, s, len + 1);
    return ((minim_object *) o);
}

minim_object *make_string(const char *s) {
    minim_string_object *o = GC_alloc(sizeof(minim_string_object));
    int len = strlen(s);
    
    o->type = MINIM_STRING_TYPE;
    o->value = GC_alloc_atomic((len + 1) * sizeof(char));
    strncpy(o->value, s, len + 1);
    return ((minim_object *) o);
}

minim_object *make_string2(char *s) {
    minim_string_object *o = GC_alloc(sizeof(minim_string_object));
    
    o->type = MINIM_STRING_TYPE;
    o->value = s;
    return ((minim_object *) o);
}

minim_object *make_pair(minim_object *car, minim_object *cdr) {
    minim_pair_object *o = GC_alloc(sizeof(minim_pair_object));
    o->type = MINIM_PAIR_TYPE;
    o->car = car;
    o->cdr = cdr;
    return ((minim_object *) o);
}

static void set_arity(proc_arity *arity, short min_arity, short max_arity) {
    if (min_arity > ARG_MAX || max_arity > ARG_MAX) {
        fprintf(stderr, "primitive procedure intialized with too large an arity: [%d, %d]", min_arity, max_arity);
        exit(1);
    } else {
        arity->arity_min = min_arity;
        arity->arity_max = max_arity;
        if (min_arity == max_arity)
            arity->type = PROC_ARITY_FIXED;
        else if (max_arity == ARG_MAX)
            arity->type = PROC_ARITY_UNBOUNDED;
        else
            arity->type = PROC_ARITY_RANGE;
    }
}

minim_object *make_prim_proc(minim_prim_proc_t proc,
                             char *name,
                             short min_arity, short max_arity) {
    minim_prim_proc_object *o = GC_alloc(sizeof(minim_prim_proc_object));
    o->type = MINIM_PRIM_PROC_TYPE;
    o->fn = proc;
    o->name = name;
    set_arity(&o->arity, min_arity, max_arity);

    return ((minim_object *) o);
}

minim_object *make_closure_proc(minim_object *args,
                                minim_object *body,
                                minim_object *env,
                                short min_arity, short max_arity) {
    minim_closure_proc_object *o = GC_alloc(sizeof(minim_closure_proc_object));
    o->type = MINIM_CLOSURE_PROC_TYPE;
    o->args = args;
    o->body = body;
    o->env = env;
    o->name = NULL;
    set_arity(&o->arity, min_arity, max_arity);
    return ((minim_object *) o);
}

static void gc_port_dtor(void *ptr) {
    minim_port_object *o = ((minim_port_object *) ptr);
    if (minim_port_is_open(o))
        fclose(minim_port(o));
}

minim_object *make_input_port(FILE *stream) {
    minim_port_object *o = GC_alloc_opt(sizeof(minim_port_object), gc_port_dtor, NULL);
    o->type = MINIM_PORT_TYPE;
    o->flags = MINIM_PORT_READ_ONLY;
    o->stream = stream;
    return ((minim_object *) o);
}

minim_object *make_output_port(FILE *stream) {
    minim_port_object *o = GC_alloc_opt(sizeof(minim_port_object), gc_port_dtor, NULL);
    o->type = MINIM_PORT_TYPE;
    o->flags = 0x0;
    o->stream = stream;
    return ((minim_object *) o);
}

minim_object *make_syntax(minim_object *e, minim_object *loc) {
    minim_syntax_object *o = GC_alloc(sizeof(minim_syntax_object));
    o->type = MINIM_SYNTAX_TYPE;
    o->e = e;
    o->loc = loc;
    return ((minim_object *) o);
}

//
//  Exceptions
//

static void arity_mismatch_exn(const char *name, proc_arity *arity, short actual) {
    if (name != NULL)
        fprintf(stderr, "%s: ", name);
    fprintf(stderr, "arity mismatch\n");

    if (proc_arity_is_fixed(arity)) {
        switch (proc_arity_min(arity)) {
        case 0:
            fprintf(stderr, " expected 0 arguments, received %d\n", actual);
            break;
        
        case 1:
            fprintf(stderr, " expected 1 argument, received %d\n", actual);
            break;

        default:
            fprintf(stderr, " expected %d arguments, received %d\n",
                            proc_arity_min(arity), actual);
        }
    } else if (proc_arity_is_unbounded(arity)) {
        switch (proc_arity_min(arity)) {
        case 0:
            fprintf(stderr, " expected at least 0 arguments, received %d\n", actual);
            break;
        
        case 1:
            fprintf(stderr, " expected at least 1 argument, received %d\n", actual);
            break;

        default:
            fprintf(stderr, " expected at least %d arguments, received %d\n",
                            proc_arity_min(arity), actual);
        }
    } else {
        fprintf(stderr, " expected between %d and %d arguments, received %d\n",
                        proc_arity_min(arity), proc_arity_max(arity), actual);
    }

    exit(1);
}

static void bad_syntax_exn(minim_object *expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object2(stderr, expr, 1, 0);
    fputc('\n', stderr);
    exit(1);
}

static void bad_type_exn(const char *name, const char *type, minim_object *x) {
    fprintf(stderr, "%s: type violation\n", name);
    fprintf(stderr, " expected: %s\n", type);
    fprintf(stderr, " received: ");
    write_object(stderr, x);
    fputc('\n', stderr);
    exit(1);
}

static int check_proc_arity(proc_arity *arity, minim_object *args, const char *name) {
    int min_arity, max_arity, argc;

    min_arity = proc_arity_min(arity);
    max_arity = proc_arity_max(arity);
    argc = 0;

    while (!minim_is_null(args)) {
        if (argc >= max_arity)
            arity_mismatch_exn(name, arity, max_arity + list_length(args));
        args = minim_cdr(args);
        ++argc;
    }

    if (argc < min_arity)
        arity_mismatch_exn(name, arity, argc);

    return argc;
}

#define check_prim_proc_arity(prim, args)   \
    check_proc_arity(&minim_prim_arity(prim), args, minim_prim_proc_name(prim))

#define check_closure_proc_arity(prim, args)    \
    check_proc_arity(&minim_closure_arity(prim), args, minim_closure_name(prim))

//
//  System
//

void set_current_dir(const char *str) {
    if (_set_current_dir(str) != 0) {
        fprintf(stderr, "could not set current directory: %s\n", str);
        exit(1);
    }

    current_directory(current_thread()) = make_string(str);
}

char *get_file_dir(const char *realpath) {
    char *dirpath;
    long i;

    for (i = strlen(realpath) - 1; i >= 0 && realpath[i] != '/'; --i);
    if (i < 0) {
        fprintf(stderr, "could not resolve directory of path %s\n", realpath);
        exit(1);
    }

    dirpath = GC_alloc_atomic((i + 2) * sizeof(char));
    strncpy(dirpath, realpath, i + 1);
    dirpath[i + 1] = '\0';
    return dirpath;
}

minim_object *load_file(const char *fname) {
    FILE *f;
    minim_object *result, *expr;
    char *old_cwd, *cwd;

    f = fopen(fname, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", fname);
        exit(1);
    }

    old_cwd = _get_current_dir();
    cwd = get_file_dir(_get_file_path(fname));
    set_current_dir(cwd);

    result = minim_void;
    while ((expr = read_object(f)) != NULL)
        result = eval_expr(expr, global_env(current_thread()));

    set_current_dir(old_cwd);
    fclose(f);
    return result;
}

//
//  Extra functions
//

// Resizes the value buffer if need be
static void resize_values_buffer(int size) {
    minim_thread *th = current_thread();
    values_buffer(th) = GC_alloc(size * sizeof(minim_object*));
    values_buffer_size(th) = size;
}

// Returns true if the object is a list
static int is_list(minim_object *x) {
    while (minim_is_pair(x)) x = minim_cdr(x);
    return minim_is_null(x);
}

// Makes an association list.
// Unsafe: only iterates on `xs`.
static minim_object *make_assoc(minim_object *xs, minim_object *ys) {
    minim_object *assoc, *it;

    if (minim_is_null(xs))
        return minim_null;

    assoc = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
    it = assoc;
    while (!minim_is_null(xs = minim_cdr(xs))) {
        ys = minim_cdr(ys);
        minim_cdr(it) = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
        it = minim_cdr(it);
    }

    return assoc;
}

// Copies a list.
// Unsafe: does not check if `xs` is a list.
static minim_object *copy_list(minim_object *xs) {
    minim_object *head, *tail, *it;

    if (minim_is_null(xs))
        return minim_null;

    head = make_pair(minim_car(xs), minim_null);
    tail = head;
    it = xs;

    while (!minim_is_null(it = minim_cdr(it))) {
        minim_cdr(tail) = make_pair(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }
    
    return head;
}

static minim_object *andmap(minim_object *proc, minim_object *lst, minim_object *env) {
    if (!minim_is_proc(proc))
        bad_type_exn("andmap", "procedure?", proc);

    while (!minim_is_null(lst)) {
        if (!minim_is_pair(lst))
            bad_type_exn("andmap", "list?", lst);
        if (minim_is_false(call_with_args(proc, make_pair(minim_car(lst), minim_null), env)))
            return minim_false;
        lst = minim_cdr(lst);
    }

    return minim_true;
}

static minim_object *ormap(minim_object *proc, minim_object *lst, minim_object *env) {
    if (!minim_is_proc(proc))
        bad_type_exn("ormap", "procedure?", proc);

    while (!minim_is_null(lst)) {
        if (!minim_is_pair(lst))
            bad_type_exn("ormap", "list?", lst);
        if (!minim_is_false(call_with_args(proc, make_pair(minim_car(lst), minim_null), env)))
            return minim_true;
        lst = minim_cdr(lst);
    }

    return minim_false;
}

// Recursively converts an object to syntax
minim_object *to_syntax(minim_object *o) {
    minim_object *it;

    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return o;
    case MINIM_NULL_TYPE:
    case MINIM_TRUE_TYPE:
    case MINIM_FALSE_TYPE:
    case MINIM_EOF_TYPE:
    case MINIM_VOID_TYPE:
    case MINIM_SYMBOL_TYPE:
    case MINIM_FIXNUM_TYPE:
    case MINIM_CHAR_TYPE:
    case MINIM_STRING_TYPE:
        return make_syntax(o, minim_false);
    
    case MINIM_PAIR_TYPE:
        it = o;
        do {
            minim_car(it) = to_syntax(minim_car(it));
            if (!minim_is_pair(minim_cdr(it))) {
                if (!minim_is_null(minim_cdr(it)))
                    minim_cdr(it) = to_syntax(minim_cdr(it));
                return make_syntax(o, minim_false);
            }
            it = minim_cdr(it);
        } while (1);
    default:
        fprintf(stderr, "datum->syntax: cannot convert to syntax");
        exit(1);
    }
}

minim_object *strip_syntax(minim_object *o) {
    switch (o->type) {
    case MINIM_SYNTAX_TYPE:
        return strip_syntax(minim_syntax_e(o));
    case MINIM_PAIR_TYPE:
        return make_pair(strip_syntax(minim_car(o)), strip_syntax(minim_cdr(o)));
    default:
        return o;
    }
}

long list_length(minim_object *xs) {
    minim_object *it = xs;
    long length = 0;

    while (!minim_is_null(it)) {
        if (!minim_is_pair(it)) {
            fprintf(stderr, "list_length: not a list");
            exit(1);
        }

        it = minim_cdr(it);
        ++length;
    }

    return length;
}

int minim_is_eq(minim_object *a, minim_object *b) {
    if (a == b)
        return 1;

    if (a->type != b->type)
        return 0;

    switch (a->type) {
    case MINIM_FIXNUM_TYPE:
        return minim_fixnum(a) == minim_fixnum(b);

    case MINIM_CHAR_TYPE:
        return minim_char(a) == minim_char(b);
    
    default:
        return 0;
    }
}

int minim_is_equal(minim_object *a, minim_object *b) {
    if (minim_is_eq(a, b))
        return 1;

    if (a->type != b->type)
        return 0;

    switch (a->type)
    {
    case MINIM_STRING_TYPE:
        return strcmp(minim_string(a), minim_string(b)) == 0;

    case MINIM_PAIR_TYPE:
        return minim_is_equal(minim_car(a), minim_car(b)) &&
               minim_is_equal(minim_cdr(a), minim_cdr(b));
    
    default:
        return 0;
    }
}


//
//  Environments
//
//  environments ::= (<frame0> <frame1> ...)
//  frames       ::= ((<var0> . <val1>) (<var1> . <val1>) ...)
//

#define SET_NAME_IF_CLOSURE(name, val) {                    \
    if (minim_is_closure_proc(val) &&                       \
       minim_closure_name(val) == NULL) {                   \
        minim_closure_name(val) = minim_symbol(name);       \
    }                                                       \
}

minim_object *make_frame(minim_object *vars, minim_object *vals) {
    return make_assoc(vars, vals);
}

void env_define_var_no_check(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame = minim_car(env);
    minim_car(env) = make_pair(make_pair(var, val), frame);
    SET_NAME_IF_CLOSURE(var, val);
}

minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame = minim_car(env);
    for (minim_object *it = frame; !minim_is_null(it); it = minim_cdr(it)) {
        minim_object *frame_var = minim_caar(frame);
        minim_object *frame_val = minim_cdar(frame);
        if (var == frame_var) {
            minim_cdar(frame) = val;
            SET_NAME_IF_CLOSURE(var, val);
            return frame_val;
        }
    }

    minim_car(env) = make_pair(make_pair(var, val), frame);
    SET_NAME_IF_CLOSURE(var, val);
    return NULL;
}

minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var) {
                minim_cdar(frame) = val;
                SET_NAME_IF_CLOSURE(var, val);
                return frame_val;
            }

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

int env_var_is_defined(minim_object *env, minim_object *var, int recursive) {
    minim_object *frame, *frame_var;

    if (minim_is_null(env))
        return 0;

    for (frame = minim_car(env); !minim_is_null(frame); frame = minim_cdr(frame)) {
        frame_var = minim_caar(frame);
        if (var == frame_var)
            return 1; 
    }

    return recursive && env_var_is_defined(minim_cdr(env), var, 1);
}

minim_object *env_lookup_var(minim_object *env, minim_object *var) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var)
                return frame_val;

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env) {
    return make_pair(make_frame(vars, vals), base_env);
}

minim_object *setup_env() {
    return extend_env(minim_null, minim_null, empty_env);
}

//
//  Primitive library
//

minim_object *is_null_proc(minim_object *args) {
    return minim_is_null(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_void_proc(minim_object *args) {
    return minim_is_void(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_eof_proc(minim_object *args) {
    return minim_is_eof(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_bool_proc(minim_object *args) {
    minim_object *o = minim_car(args);
    return (minim_is_true(o) || minim_is_false(o)) ? minim_true : minim_false;
}

minim_object *is_symbol_proc(minim_object *args) {
    return minim_is_symbol(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_fixnum_proc(minim_object *args) {
    return minim_is_fixnum(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_char_proc(minim_object *args) {
    return minim_is_char(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_string_proc(minim_object *args) {
    return minim_is_string(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_pair_proc(minim_object *args) {
    return minim_is_pair(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_list_proc(minim_object *args) {
    minim_object *thing;
    for (thing = minim_car(args); minim_is_pair(thing); thing = minim_cdr(thing));
    return minim_is_null(thing) ? minim_true : minim_false;
}

minim_object *is_procedure_proc(minim_object *args) {
    minim_object *o = minim_car(args);
    return (minim_is_prim_proc(o) || minim_is_closure_proc(o)) ? minim_true : minim_false;
}

minim_object *is_input_port_proc(minim_object *args) {
    return minim_is_input_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_output_port_proc(minim_object *args) {
    return minim_is_output_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_syntax_proc(minim_object *args) {
    return minim_is_syntax(minim_car(args)) ? minim_true : minim_false;
}

minim_object *char_to_integer_proc(minim_object *args) {
    return make_fixnum(minim_char(minim_car(args)));
}

minim_object *integer_to_char_proc(minim_object *args) {
    return make_char(minim_fixnum(minim_car(args)));
}

minim_object *number_to_string_proc(minim_object *args) {
    char buffer[30];
    sprintf(buffer, "%ld", minim_fixnum(minim_car(args)));
    return make_string(buffer);
}

minim_object *string_to_number_proc(minim_object *args) {
    // TODO: unchecked conversion
    return make_fixnum(atoi(minim_string(minim_car(args))));
}

minim_object *symbol_to_string_proc(minim_object *args) {
    return make_string(minim_symbol(minim_car(args)));
}

minim_object *string_to_symbol_proc(minim_object *args) {
    return intern(minim_string(minim_car(args)));
}

minim_object *eq_proc(minim_object *args) {
    minim_object *a = minim_car(args);
    minim_object *b = minim_cadr(args);
    return minim_is_eq(a, b) ? minim_true : minim_false;
}

minim_object *equal_proc(minim_object *args) {
    minim_object *a = minim_car(args);
    minim_object *b = minim_cadr(args);
    return minim_is_equal(a, b) ? minim_true : minim_false;
}

minim_object *void_proc(minim_object *args) {
    return minim_void;
}

minim_object *call_with_values_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *values_proc(minim_object *args) {
    minim_thread *th;
    long i, len;

    th = current_thread();
    len = list_length(args);
    if (len > values_buffer_size(th))
        resize_values_buffer(len);
    
    values_buffer_count(th) = len;
    for (i = 0; i < len; ++i) {
        values_buffer_ref(th, i) = minim_car(args);
        args = minim_cdr(args);
    }
    
    return minim_values;
}

minim_object *not_proc(minim_object *args) {
    return minim_is_false(minim_car(args)) ? minim_true : minim_false;   
}

minim_object *cons_proc(minim_object *args) {
    return make_pair(minim_car(args), minim_cadr(args));
}

minim_object *car_proc(minim_object *args) {
    return minim_caar(args);
}

minim_object *cdr_proc(minim_object *args) {
    return minim_cdar(args);
}

minim_object *set_car_proc(minim_object *args) {
    minim_caar(args) = minim_cadr(args);
    return minim_void;
}

minim_object *set_cdr_proc(minim_object *args) {
    minim_cdar(args) = minim_cadr(args);
    return minim_void;
}

minim_object *list_proc(minim_object *args) {
    return args;
}

minim_object *andmap_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    exit(1);
}

minim_object *ormap_proc(minim_object *args) {
    fprintf(stderr, "ormap: should not be called directly");
    exit(1);
}

minim_object *add_proc(minim_object *args) {
    long result = 0;

    while (!minim_is_null(args)) {
        result += minim_fixnum(minim_car(args));
        args = minim_cdr(args);
    }

    return make_fixnum(result);
}

minim_object *sub_proc(minim_object *args) {
    long result;
    
    if (minim_is_null(minim_cdr(args))) {
        result = -(minim_fixnum(minim_car(args)));
    } else {
        result = minim_fixnum(minim_car(args));
        args = minim_cdr(args);
        while (!minim_is_null(args)) {
            result -= minim_fixnum(minim_car(args));
            args = minim_cdr(args);
        }
    }

    return make_fixnum(result);
}

minim_object *mul_proc(minim_object *args) {
    long result = 1;

    while (!minim_is_null(args)) {
        result *= minim_fixnum(minim_car(args));
        args = minim_cdr(args);
    }

    return make_fixnum(result);
}

minim_object *div_proc(minim_object *args) {
    return make_fixnum(minim_fixnum(minim_car(args)) /
                       minim_fixnum(minim_cadr(args)));
}

minim_object *remainder_proc(minim_object *args) {
    return make_fixnum(minim_fixnum(minim_car(args)) %
                       minim_fixnum(minim_cadr(args)));
}

minim_object *modulo_proc(minim_object *args) {
    long result = minim_fixnum(minim_car(args)) % minim_fixnum(minim_cadr(args));
    if ((minim_fixnum(minim_car(args)) < 0) != (minim_fixnum(minim_cadr(args)) < 0))
        result += minim_fixnum(minim_cadr(args));
    return make_fixnum(result);
}

minim_object *number_eq_proc(minim_object *args) { 
    return (minim_fixnum(minim_car(args)) == minim_fixnum(minim_cadr(args))) ?
           minim_true :
           minim_false;
}

minim_object *number_ge_proc(minim_object *args) { 
    return (minim_fixnum(minim_car(args)) >= minim_fixnum(minim_cadr(args))) ?
           minim_true :
           minim_false;
}

minim_object *number_le_proc(minim_object *args) { 
    return (minim_fixnum(minim_car(args)) <= minim_fixnum(minim_cadr(args))) ?
           minim_true :
           minim_false;
}

minim_object *number_gt_proc(minim_object *args) { 
    return (minim_fixnum(minim_car(args)) > minim_fixnum(minim_cadr(args))) ?
           minim_true :
           minim_false;
}

minim_object *number_lt_proc(minim_object *args) { 
    return (minim_fixnum(minim_car(args)) < minim_fixnum(minim_cadr(args))) ?
           minim_true :
           minim_false;
}

minim_object *make_string_proc(minim_object *args) {
    char *str;
    long len, i;
    char c;

    len = minim_fixnum(minim_car(args));
    c = minim_is_null(minim_cdr(args)) ? 'a' : minim_char(minim_cadr(args));

    str = GC_alloc_atomic((len + 1) * sizeof(char));    
    for (i = 0; i < len; ++i)
        str[i] = c;
    str[i] = '\0';

    return make_string2(str);
}

minim_object *string_proc(minim_object *args) {
    long len, i;
    char *str;

    len = list_length(args);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (i = 0; i < len; ++i) {
        str[i] = minim_char(minim_car(args));
        args = minim_cdr(args);
    }
    str[i] = '\0';

    return make_string2(str);
}

minim_object *string_length_proc(minim_object *args) {
    char *str = minim_string(minim_car(args));
    return make_fixnum(strlen(str));
}

minim_object *string_ref_proc(minim_object *args) {
    char *str;
    long len, idx;

    str = minim_string(minim_car(args));
    idx = minim_fixnum(minim_cadr(args));
    len = strlen(str);

    if (idx >= len) {
        fprintf(stderr, "string-ref: index out of bounds\n");
        fprintf(stderr, " string: %s, length: %ld, index %ld\n", str, len, idx);
        exit(1);
    }

    return make_char((int) str[idx]);
}

minim_object *string_set_proc(minim_object *args) {
    char *str;
    long len, idx;
    char c;

    str = minim_string(minim_car(args));
    len = strlen(str);
    idx = minim_fixnum(minim_cadr(args));
    c = minim_char(minim_car(minim_cddr(args)));

    if (idx >= len) {
        fprintf(stderr, "string-set!: index out of bounds\n");
        fprintf(stderr, " string: %s, length: %ld, index %ld\n", str, len, idx);
        exit(1);
    }

    str[idx] = c;
    return minim_void;
}

minim_object *syntax_e_proc(minim_object *args) {
    return minim_syntax_e(minim_car(args));
}

minim_object *syntax_loc_proc(minim_object *args) {
    return minim_syntax_loc(minim_car(args));
}

minim_object *to_syntax_proc(minim_object *args) {
    return to_syntax(minim_car(args));
}

minim_object *eval_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    exit(1);
}

minim_object *apply_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    exit(1);
}

minim_object *interaction_environment_proc(minim_object *args) {
    return global_env(current_thread());
}

minim_object *empty_environment_proc(minim_object *args) {
    return setup_env();
}

minim_object *environment_proc(minim_object *args) {
    return make_env();
}

minim_object *extend_environment_proc(minim_object *args) {
    return make_pair(minim_null, minim_car(args));
}

minim_object *environment_variable_value_proc(minim_object *args) {
    minim_object *env, *name;

    env = minim_car(args);
    name = minim_cadr(args);

    if (!minim_is_symbol(name)) {
        bad_type_exn("environment-variable-value", "symbol?", name);
    } else if (env_var_is_defined(env, name, 0)) {
        // variable found
        return env_lookup_var(env, name);
    } else {
        // variable not found
        if (minim_is_null(minim_cddr(args))) {
            // default exception
            fprintf(stderr, "environment-variable-value: variable not bound");
            fprintf(stderr, " name: %s", minim_symbol(name));
        } else {
            // custom exception
            minim_object *exn, *env, *expr;

            exn = minim_car(minim_cddr(args));
            if (minim_is_prim_proc(exn)) {
                check_prim_proc_arity(exn, minim_null);
                return (minim_prim_proc(exn))(minim_null);
            } else if (minim_is_closure_proc(exn)) {
                check_closure_proc_arity(exn, minim_null);
                env = extend_env(minim_null, minim_null, minim_closure_env(exn));
                expr = minim_closure_body(exn);
                return eval_expr(expr, env);
            } else {
                bad_type_exn("environment-variable-value", "procedure?", exn);
            }
        }
    }

    fprintf(stderr, "unreachable");
    return minim_void;
}

minim_object *environment_set_variable_value_proc(minim_object *args) {
    minim_object *env, *name, *val;

    env = minim_car(args);
    name = minim_cadr(args);
    val = minim_car(minim_cddr(args));

    if (!minim_is_symbol(name))
        bad_type_exn("environment-variable-value", "symbol?", name);

    env_define_var(env, name, val);
    return minim_void;
}

minim_object *current_environment_proc(minim_object *args) {
    fprintf(stderr, "current-environment: should not be called directly");
    exit(1);
}

minim_object *current_input_port_proc(minim_object *args) {
    return input_port(current_thread());
}

minim_object *current_output_port_proc(minim_object *args) {
    return output_port(current_thread());
}

minim_object *open_input_port_proc(minim_object *args) {
    FILE *stream;
    minim_object *port;

    stream = fopen(minim_string(minim_car(args)), "r");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    port = make_input_port(stream);
    minim_port_set(port, MINIM_PORT_OPEN);
    return port;
}

minim_object *open_output_port_proc(minim_object *args) {
    FILE *stream;
    minim_object *port;
    
    stream = fopen(minim_string(minim_car(args)), "w");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    port = make_input_port(stream);
    minim_port_set(port, MINIM_PORT_OPEN);
    return port;
}

minim_object *close_input_port_proc(minim_object *args) {
    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *close_output_port_proc(minim_object *args) {
    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *read_proc(minim_object *args) {
    minim_object *in_p, *o;

    in_p = minim_is_null(args) ? input_port(current_thread()) : minim_car(args);
    o = read_object(minim_port(in_p));
    return (o == NULL) ? minim_eof : o;
}

minim_object *read_char_proc(minim_object *args) {
    minim_object *in_p;
    int ch;
    
    in_p = minim_is_null(args) ? input_port(current_thread()) : minim_car(args);
    ch = getc(minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *peek_char_proc(minim_object *args) {
    minim_object *in_p;
    int ch;
    
    in_p = minim_is_null(args) ? input_port(current_thread()) : minim_car(args);
    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *char_is_ready_proc(minim_object *args) {
    minim_object *in_p;
    int ch;

    in_p = minim_is_null(args) ? input_port(current_thread()) : minim_car(args);
    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_false : minim_true;
}

minim_object *display_proc(minim_object *args) {
    minim_object *out_p, *o;

    o = minim_car(args);
    out_p = minim_is_null(minim_cdr(args)) ? output_port(current_thread()) : minim_cadr(args);
    write_object2(minim_port(out_p), o, 0, 1);
    return minim_void;
}

minim_object *write_proc(minim_object *args) {
    minim_object *out_p, *o;

    o = minim_car(args);
    out_p = minim_is_null(minim_cdr(args)) ? output_port(current_thread()) : minim_cadr(args);
    write_object(minim_port(out_p), o);
    return minim_void;
}

minim_object *write_char_proc(minim_object *args) {
    minim_object *out_p, *ch;

    ch = minim_car(args);
    out_p = minim_is_null(minim_cdr(args)) ? output_port(current_thread()) : minim_cadr(args);
    putc(minim_char(ch), minim_port(out_p));
    return minim_void;
}

minim_object *newline_proc(minim_object *args) {
    minim_object *out_p = minim_is_null(args) ? output_port(current_thread()) : minim_car(args);
    putc('\n', minim_port(out_p));
    return minim_void;
}

minim_object *load_proc(minim_object *args) {
    return load_file(minim_string(minim_car(args)));
}

minim_object *error_proc(minim_object *args) {
    while (!minim_is_null(args)) {
        write_object2(stderr, minim_car(args), 1, 1);
        fprintf(stderr, " ");
        args = minim_cdr(args);
    }

    printf("\n");
    exit(1);
}

minim_object *exit_proc(minim_object *args) {
    exit(0);
}

minim_object *syntax_error_proc(minim_object *args) {
    minim_object *what, *why, *where, *sub;

    what = minim_car(args);
    why = minim_cadr(args);

    if (!minim_is_symbol(what))
        bad_type_exn("syntax-error", "symbol?", what);
    if (!minim_is_string(why))
        bad_type_exn("syntax-error", "string?", why);

    fprintf(stderr, "%s: %s\n", minim_symbol(what), minim_string(why));
    if (!minim_is_null(minim_cddr(args))) {
        where = minim_car(minim_cddr(args));
        if (!minim_is_null(minim_cdr(minim_cddr(args)))) {
            sub = minim_cadr(minim_cddr(args));
            fputs("  at: ", stderr);
            write_object2(stderr, sub, 1, 1);
            fprintf(stderr, "\n");
        }

        fputs("  in: ", stderr);
        write_object2(stderr, where, 1, 1);
        fprintf(stderr, "\n");
    }

    exit(1);
}

minim_object *current_directory_proc(minim_object *args) {
    minim_object *path;

    if (minim_is_null(args)) {
        // getter
        return current_directory(current_thread());
    } else {
        // setter
        path = minim_car(args);
        if (!minim_is_string(path))
            bad_type_exn("current-directory", "string?", path);
        
        set_current_dir(minim_string(path));
        return minim_void;
    }
}

minim_object *boot_expander_proc(minim_object *args) {
    minim_object *val;
    minim_thread *th;

    th = current_thread();
    if (minim_is_null(args)) {
        // getter
        return boot_expander(th);
    } else {
        // setter
        val = minim_car(args);
        if (!minim_is_bool(val))
            bad_type_exn("boot-expander?", "boolean?", val);
        boot_expander(th) = val;
        return minim_void;
    }
}

//
//  Syntax check
//

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum>)
static void check_1ary_syntax(minim_object *expr) {
    minim_object *rest = minim_cdr(expr);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> <datum>)
static void check_2ary_syntax(minim_object *expr) {
    minim_object *rest;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> <datum> <datum>)
static void check_3ary_syntax(minim_object *expr) {
    minim_object *rest;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr must be `(<name> <symbol> <datum>)`
static void check_assign(minim_object *expr) {
    minim_object *rest;

    rest = minim_cdr(expr);
    if (!minim_is_pair(rest) || !minim_is_symbol(minim_car(rest)))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    if (!minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be either
//  (ii) `(<name> (<symbol> ...) <datum> ...)
//  (i)  `(<name> <symbol> <datum>)`
static void check_define(minim_object *expr) {
    minim_object *rest, *id;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    id = minim_car(rest);
    rest = minim_cdr(rest);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    if (minim_is_symbol(id) && !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> (<symbol> ...) <datum>)
static void check_define_values(minim_object *expr) {
    minim_object *rest, *ids;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    ids = minim_car(rest);
    while (minim_is_pair(ids)) {
        if (!minim_is_symbol(minim_car(ids)))
            bad_syntax_exn(expr);
        ids = minim_cdr(ids);
    } 

    if (!minim_is_null(ids))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(cond . <???>)`
// Check: `expr` must be `(cond [<test> <clause> ...] ...)`
// Does not check if each `<clause>` is an expression.
// Does not check if each `<clause> ...` forms a list.
// Checks that only `else` appears in the tail position
static void check_cond(minim_object *expr) {
    minim_object *rest, *datum;

    rest = minim_cdr(expr);
    while (minim_is_pair(rest)) {
        datum = minim_car(rest);
        if (!minim_is_pair(datum) || !minim_is_pair(minim_cdr(datum)))
            bad_syntax_exn(expr);

        datum = minim_car(datum);
        if (minim_is_symbol(datum) &&
            minim_car(datum) == else_symbol &&
            !minim_is_null(minim_cdr(rest))) {
            fprintf(stderr, "cond: else clause must be last");
            fprintf(stderr, " at: ");
            write_object2(stderr, expr, 1, 0);
            fputc('\n', stderr);
            exit(1);
        }

        rest = minim_cdr(rest);
    }

    if (!minim_is_null(rest))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let ([<var> <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let(minim_object *expr) {
    minim_object *bindings, *bind;
    
    bindings = minim_cdr(expr);
    if (!minim_is_pair(bindings) || !minim_is_pair(minim_cdr(bindings)))
        bad_syntax_exn(expr);
    
    bindings = minim_car(bindings);
    while (minim_is_pair(bindings)) {
        bind = minim_car(bindings);
        if (!minim_is_pair(bind) || !minim_is_symbol(minim_car(bind)))
            bad_syntax_exn(expr);

        bind = minim_cdr(bind);
        if (!minim_is_pair(bind) || !minim_is_null(minim_cdr(bind)))
            bad_syntax_exn(expr);

        bindings = minim_cdr(bindings);
    }

    if (!minim_is_null(bindings))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let-values ([(<var> ...) <val>] ...) <body> ...)`
// Does not check if each `<body>` is an expression.
// Does not check if `<body> ...` forms a list.
static void check_let_values(minim_object *expr) {
    minim_object *bindings, *bind, *ids;
    
    bindings = minim_cdr(expr);
    if (!minim_is_pair(bindings) || !minim_is_pair(minim_cdr(bindings)))
        bad_syntax_exn(expr);
    
    bindings = minim_car(bindings);
    while (minim_is_pair(bindings)) {
        bind = minim_car(bindings);
        if (!minim_is_pair(bind))
            bad_syntax_exn(expr);

        ids = minim_car(bind);
        while (minim_is_pair(ids)) {
            if (!minim_is_symbol(minim_car(ids)))
                bad_syntax_exn(expr);
            ids = minim_cdr(ids);
        } 

        if (!minim_is_null(ids))
            bad_syntax_exn(expr);

        bind = minim_cdr(bind);
        if (!minim_is_pair(bind) || !minim_is_null(minim_cdr(bind)))
            bad_syntax_exn(expr);

        bindings = minim_cdr(bindings);
    }

    if (!minim_is_null(bindings))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(let . <???>)`
// Check: `expr` must be `(let <name> ([<var> <val>] ...) <body> ...)`
// Just check that <name> is a symbol and then call
// `check_loop` starting at (<name> ...)
static void check_let_loop(minim_object *expr) {
    if (!minim_is_pair(minim_cdr(expr)) && !minim_is_symbol(minim_cadr(expr)))
        bad_syntax_exn(expr);
    check_let(minim_cdr(expr));
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <datum> ...)`
static void check_begin(minim_object *expr) {
    minim_object *rest = minim_cdr(expr);

    while (minim_is_pair(rest))
        rest = minim_cdr(rest);

    if (!minim_is_null(rest))
        bad_syntax_exn(expr);
}

static void check_lambda(minim_object *expr, short *min_arity, short *max_arity) {
    minim_object *args = minim_cadr(expr);
    int argc = 0;

    while (minim_is_pair(args)) {
        if (!minim_is_symbol(minim_car(args))) {
            fprintf(stderr, "expected a identifier for an argument:\n ");
            write_object(stderr, expr);
            fputc('\n', stderr);
            exit(1);
        }

        args = minim_cdr(args);
        ++argc;
    }

    if (minim_is_null(args)) {
        *min_arity = argc;
        *max_arity = argc;
    } else if (minim_is_symbol(args)) {
        *min_arity = argc;
        *max_arity = ARG_MAX;
    } else {
        fprintf(stderr, "expected an identifier for the rest argument:\n ");
        write_object(stderr, expr);
        fputc('\n', stderr);
        exit(1);
    }
}

//
//  Evaluation
//

static minim_object *let_vars(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_caar(bindings),
                      let_vars(minim_cdr(bindings))));
}

static minim_object *let_vals(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_car(minim_cdar(bindings)),
                      let_vals(minim_cdr(bindings))));
}

// (define (<name> . <formals>) . <body>)
// => (define <name> (lambda <formals> . <body>))
// (define <name> <body>)
// => (define-values (<name>) <body>)
static minim_object *expand_define(minim_object *define) {
    minim_object *id, *rest;

    id = minim_cadr(define);
    rest = minim_cddr(define);
    if (minim_is_symbol(id)) {
        // value
        return make_pair(define_values_symbol,
               make_pair(make_pair(id, minim_null),
               rest));
    } else {
        minim_object *name, *formals, *body;

        // procedure
        name = minim_car(minim_cadr(define));
        formals = minim_cdr(minim_cadr(define));
        body = minim_cddr(define);

        return expand_define(
                make_pair(define_symbol,
                    make_pair(name,
                    make_pair(
                            make_pair(lambda_symbol,
                            make_pair(formals,
                            body)),
                    minim_null))));
    }
}

// (let ((<var> <val>) ...) . <body>)
// => (let-values (((<var>) <val>) ...) . <body>)
static minim_object *expand_let(minim_object *form, minim_object *let) {
    minim_object *vars, *vals, *bindings, *bind, *it;

    vars = let_vars(minim_cadr(let));
    vals = let_vals(minim_cadr(let));
    if (minim_is_null(vars)) {
        bindings = minim_null;
    } else {
        bind = make_pair(make_pair(minim_car(vars), minim_null), make_pair(minim_car(vals), minim_null));
        bindings = make_pair(bind, minim_null);
        vars = minim_cdr(vars);
        vals = minim_cdr(vals);
        for (it = bindings; !minim_is_null(vars); vars = minim_cdr(vars), vals = minim_cdr(vals)) {
            bind = make_pair(make_pair(minim_car(vars), minim_null), make_pair(minim_car(vals), minim_null));
            minim_cdr(it) = make_pair(bind, minim_null);
            it = minim_cdr(it);
        }
    }

    return make_pair(form, make_pair(bindings, minim_cddr(let)));
}

// (let <name> ((<var> <val>) ...) . <body>)
// => (letrec-values (((<name>) (lambda (<var> ...) . <body>))) (<name> <val> ...))
static minim_object *expand_let_loop(minim_object *let) {
    minim_object *name, *formals, *vals, *body;

    name = minim_cadr(let);
    formals = let_vars(minim_car(minim_cddr(let)));
    vals = let_vals(minim_car(minim_cddr(let)));
    body = minim_cdr(minim_cddr(let));

    return make_pair(letrec_values_symbol,
           make_pair(                                   // expr rib                      
                make_pair(                              // bindings
                    make_pair(                          // binding
                        make_pair(name, minim_null),    // name 
                    make_pair(                          // value
                        make_pair(lambda_symbol,
                        make_pair(formals,
                        body)),
                    minim_null)),
                minim_null),
           make_pair(make_pair(name, vals),             // call
           minim_null)));
}

static minim_object *apply_args(minim_object *args) {
    minim_object *head, *tail, *it;

    if (minim_is_null(minim_cdr(args))) {
        if (!is_list(minim_car(args)))
            bad_type_exn("apply", "list?", minim_car(args));

        return minim_car(args);
    }

    head = make_pair(minim_car(args), minim_null);
    tail = head;
    it = args;

    while (!minim_is_null(minim_cdr(it = minim_cdr(it)))) {
        minim_cdr(tail) = make_pair(minim_car(it), minim_null);
        tail = minim_cdr(tail);
    }

    if (!is_list(minim_car(it)))
        bad_type_exn("apply", "list?", minim_car(it));

    minim_cdr(tail) = minim_car(it);
    return head;
}

static minim_object *eval_exprs(minim_object *exprs, minim_object *env) {
    minim_object *head, *tail, *it, *result;
    minim_thread *th;

    if (minim_is_null(exprs))
        return minim_null;

    head = NULL; tail = NULL;
    for (it = exprs; !minim_is_null(it); it = minim_cdr(it)) {
        result = eval_expr(minim_car(it), env);
        if (minim_is_values(result)) {
            th = current_thread();
            if (values_buffer_count(th) != 1) {
                fprintf(stderr, "result arity mismatch\n");
                fprintf(stderr, "  expected: 1, received: %d\n", values_buffer_count(th));
                exit(1);
            } else {
                result = values_buffer_ref(th, 0);
            }
        }

        if (head == NULL) {
            head = make_pair(result, minim_null);
            tail = head;
        } else {
            minim_cdr(tail) = make_pair(result, minim_null);   
            tail = minim_cdr(tail);
        }
    }

    return head;
}

static minim_object *call_with_values(minim_object *producer,
                                      minim_object *consumer,
                                      minim_object *env) {
    minim_object *produced, *args, *tail;
    minim_thread *th;

    produced = call_with_args(producer, minim_null, env);
    if (minim_is_values(produced)) {
        // need to unpack
        th = current_thread();
        if (values_buffer_count(th) == 0) {
            args = minim_null;
        } else if (values_buffer_count(th) == 1) {
            args = make_pair(values_buffer_ref(th, 0), minim_null);
        } else if (values_buffer_count(th) == 2) {
            args = make_pair(values_buffer_ref(th, 0),
                   make_pair(values_buffer_ref(th, 1), minim_null));
        } else {
            // slow path
            args = make_pair(values_buffer_ref(th, 0), minim_null);
            tail = args;
            for (int i = 1; i < values_buffer_count(th); ++i) {
                minim_cdr(tail) = make_pair(values_buffer_ref(th, i), minim_null);
                tail = minim_cdr(tail);
            }
        }
    } else {
        args = make_pair(produced, minim_null);
    }

    return call_with_args(consumer, args, env);
}

static minim_object *call_with_args(minim_object *proc, minim_object *args, minim_object *env) {
    minim_object *expr;

application:

    if (minim_is_prim_proc(proc)) {
        check_prim_proc_arity(proc, args);

        // special case for `eval`
        if (minim_prim_proc(proc) == eval_proc) {
            expr = minim_car(args);
            if (!minim_is_null(minim_cdr(args)))
                env = minim_cadr(args);
            return eval_expr(expr, env);
        }

        // special case for `apply`
        if (minim_prim_proc(proc) == apply_proc) {
            proc = minim_car(args);
            args = apply_args(minim_cdr(args));
            goto application;
        }

        // special case for `current-environment`
        if (minim_prim_proc(proc) == current_environment_proc) {
            return env;
        }

        // special case for `andmap`
        if (minim_prim_proc(proc) == andmap_proc) {
            return andmap(minim_car(args), minim_cadr(args), env);
        }

        // special case for `ormap`
        if (minim_prim_proc(proc) == ormap_proc) {
            return ormap(minim_car(args), minim_cadr(args), env);
        }

        return minim_prim_proc(proc)(args);
    } else if (minim_is_closure_proc(proc)) {
        minim_object *vars;

        // check arity and extend environment
        check_closure_proc_arity(proc, args);
        env = extend_env(minim_null, minim_null, minim_closure_env(proc));

        // process args
        vars = minim_closure_args(proc);
        while (minim_is_pair(vars)) {
            env_define_var_no_check(env, minim_car(vars), minim_car(args));
            vars = minim_cdr(vars);
            args = minim_cdr(args);
        }

        // optionally process rest argument
        if (minim_is_symbol(vars))
            env_define_var_no_check(env, vars, copy_list(args));

        // tail call (not really)
        expr = minim_closure_body(proc);
        return eval_expr(expr, env);
    } else {
        fprintf(stderr, "not a procedure\n");
        exit(1);
    }
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {

loop:

    if (minim_is_true(expr) || minim_is_false(expr) ||
        minim_is_fixnum(expr) ||
        minim_is_char(expr) ||
        minim_is_string(expr)) {
        // self-evaluating
        return expr;
    } else if (minim_is_symbol(expr)) {
        // variable
        return env_lookup_var(env, expr);
    } else if (minim_is_null(expr)) {
        fprintf(stderr, "missing procedure expression\n");
        fprintf(stderr, "  in: ");
        write_object2(stderr, expr, 0, 0);
        exit(1);
    } else if (minim_is_pair(expr)) {
        minim_object *proc, *head, *args, *result, *it, *bindings, *bind, *env2;
        minim_thread *th;
        long var_count, idx;

        // special forms
        head = minim_car(expr);
        if (minim_is_symbol(head)) {
            if (head == define_values_symbol) {
                // define-values form
                check_define_values(expr);
                var_count = list_length(minim_cadr(expr));
                result = eval_expr(minim_car(minim_cddr(expr)), env);
                if (minim_is_values(result)) {;
                    // multi-valued
                    th = current_thread();
                    if (var_count != values_buffer_count(th)) {
                        fprintf(stderr, "result arity mismatch\n");
                        fprintf(stderr, "  expected: %ld, received: %d\n",
                                        var_count, values_buffer_count(th));
                        exit(1);
                    }

                    idx = 0;
                    for (it = minim_cadr(expr); !minim_is_null(it); it = minim_cdr(it), ++idx)
                        env_define_var(env, minim_car(it), values_buffer_ref(th, idx));
                } else {
                    // single-valued
                    if (var_count != 1) {
                        fprintf(stderr, "result arity mismatch\n");
                        fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                        exit(1);
                    }
                    
                    env_define_var(env, minim_car(minim_cadr(expr)), result);
                }

                GC_REGISTER_LOCAL_ARRAY(expr);
                return minim_void;
            } else if (head == let_values_symbol) {
                // let-values form
                check_let_values(expr);
                env2 = extend_env(minim_null, minim_null, env);
                for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_is_values(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th)) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: %d\n",
                                            var_count, values_buffer_count(th));
                            exit(1);
                        }

                        idx = 0;
                        for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx)
                            env_define_var(env2, minim_car(it), values_buffer_ref(th, idx));
                    } else {
                        // single-valued
                        if (var_count != 1) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                            exit(1);
                        }
                        
                        env_define_var(env2, minim_caar(bind), result);
                    }
                }
                
                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                env = env2;
                goto loop;
            } else if (head == letrec_values_symbol) {
                // letrec-values
                minim_object *to_bind;

                check_let_values(expr);
                to_bind = minim_null;
                env = extend_env(minim_null, minim_null, env);
                for (bindings = minim_cadr(expr); !minim_is_null(bindings); bindings = minim_cdr(bindings)) {
                    bind = minim_car(bindings);
                    var_count = list_length(minim_car(bind));
                    result = eval_expr(minim_cadr(bind), env);
                    if (minim_is_values(result)) {
                        // multi-valued
                        th = current_thread();
                        if (var_count != values_buffer_count(th)) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: %d\n",
                                            var_count, values_buffer_count(th));
                            exit(1);
                        }

                        idx = 0;
                        for (it = minim_car(bind); !minim_is_null(it); it = minim_cdr(it), ++idx)
                            to_bind = make_pair(make_pair(minim_car(it), values_buffer_ref(th, idx)), to_bind);
                    } else {
                        // single-valued
                        if (var_count != 1) {
                            fprintf(stderr, "result arity mismatch\n");
                            fprintf(stderr, "  expected: %ld, received: 1\n", var_count);
                            exit(1);
                        }
                        
                        to_bind = make_pair(make_pair(minim_caar(bind), result), to_bind);
                    }
                }

                for (it = to_bind; !minim_is_null(it); it = minim_cdr(it))
                    env_define_var(env, minim_caar(it), minim_cdar(it));

                expr = make_pair(begin_symbol, (minim_cddr(expr)));
                goto loop;
            } else if (head == quote_symbol) {
                // quote form
                check_1ary_syntax(expr);
                return minim_cadr(expr);
            } else if (head == syntax_symbol || head == quote_syntax_symbol) {
                // quote-syntax form
                check_1ary_syntax(expr);
                if (minim_is_syntax(minim_cadr(expr)))
                    return minim_cadr(expr);
                else
                    return make_syntax(minim_cadr(expr), minim_false);
            } else if (head == syntax_loc_symbol) {
                // syntax/loc form
                check_2ary_syntax(expr);
                return make_syntax(minim_car(minim_cddr(expr)), minim_cadr(expr));
            } else if (head == setb_symbol) {
                // set! form
                check_assign(expr);
                env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
                return minim_void;
            } else if (head == if_symbol) {
                // if form
                check_3ary_syntax(expr);
                expr = (minim_is_false(eval_expr(minim_cadr(expr), env)) ?
                       minim_cadr(minim_cddr(expr)) :
                       minim_car(minim_cddr(expr)));
                goto loop;
            } else if (head == lambda_symbol) {
                short min_arity, max_arity;

                // lambda form
                check_lambda(expr, &min_arity, &max_arity);
                return make_closure_proc(minim_cadr(expr),
                                        make_pair(begin_symbol, minim_cddr(expr)),
                                        env,
                                        min_arity,
                                        max_arity);
            } else if (head == begin_symbol) {
                // begin form
                check_begin(expr);
                expr = minim_cdr(expr);
                if (minim_is_null(expr))
                    return minim_void;

                while (!minim_is_null(minim_cdr(expr))) {
                    eval_expr(minim_car(expr), env);
                    expr = minim_cdr(expr);
                }

                expr = minim_car(expr);
                goto loop;
            } else if (minim_is_true(boot_expander(current_thread()))) {
                // expanded forms
                if (head == define_symbol) {
                    // define form
                    check_define(expr);
                    expr = expand_define(expr);
                    goto loop;
                } else if (head == let_symbol) {
                    if (minim_is_pair(minim_cdr(expr)) && minim_is_symbol(minim_cadr(expr))) {
                        // let loop form
                        check_let_loop(expr);
                        expr = expand_let_loop(expr);
                        goto loop;
                    } else {
                        // let form
                        check_let(expr);
                        expr = expand_let(let_values_symbol, expr);
                        goto loop;
                    }
                } else if (head == letrec_symbol) {
                    // letrec form
                    check_let(expr);
                    expr = expand_let(letrec_values_symbol, expr);
                    goto loop;
                } else if (head == cond_symbol) {
                    // cond form
                    check_cond(expr);
                    expr = minim_cdr(expr);
                    while (!minim_is_null(expr)) {
                        if (minim_caar(expr) == else_symbol) {
                            if (!minim_is_null(minim_cdr(expr))) {
                                fprintf(stderr, "else clause must be last");
                                exit(1);
                            }
                            expr = make_pair(begin_symbol, minim_cdar(expr));
                            goto loop;
                        } else if (!minim_is_false(eval_expr(minim_caar(expr), env))) {
                            expr = make_pair(begin_symbol, minim_cdar(expr));
                            goto loop;
                        }
                        expr = minim_cdr(expr);
                    }

                    return minim_void;
                } else if (head == and_symbol) {
                    // and form
                    check_begin(expr);
                    expr = minim_cdr(expr);
                    if (minim_is_null(expr))
                        return minim_true;

                    while (!minim_is_null(minim_cdr(expr))) {
                        result = eval_expr(minim_car(expr), env);
                        if (minim_is_false(result))
                            return result;
                        expr = minim_cdr(expr);
                    }

                    expr = minim_car(expr);
                    goto loop;
                } else if (head == or_symbol) {
                    // or form
                    check_begin(expr);
                    expr = minim_cdr(expr);
                    if (minim_is_null(expr))
                        return minim_false;

                    while (!minim_is_null(minim_cdr(expr))) {
                        result = eval_expr(minim_car(expr), env);
                        if (!minim_is_false(result))
                            return result;
                        expr = minim_cdr(expr);
                    }

                    expr = minim_car(expr);
                    goto loop;
                }
            } 
        }
        
        proc = eval_expr(head, env);
        args = eval_exprs(minim_cdr(expr), env);

application:

        if (minim_is_prim_proc(proc)) {
            check_prim_proc_arity(proc, args);

            // special case for `eval`
            if (minim_prim_proc(proc) == eval_proc) {
                expr = minim_car(args);
                if (!minim_is_null(minim_cdr(args)))
                    env = minim_cadr(args);
                goto loop;
            }

            // special case for `apply`
            if (minim_prim_proc(proc) == apply_proc) {
                proc = minim_car(args);
                args = apply_args(minim_cdr(args));
                goto application;
            }

            // special case for `current-environment`
            if (minim_prim_proc(proc) == current_environment_proc) {
                return env;
            }

            // special case for `andmap`
            if (minim_prim_proc(proc) == andmap_proc) {
                return andmap(minim_car(args), minim_cadr(args), env);
            }

            // special case for `ormap`
            if (minim_prim_proc(proc) == ormap_proc) {
                return ormap(minim_car(args), minim_cadr(args), env);
            }

            // special case for `call-with-values`
            if (minim_prim_proc(proc) == call_with_values_proc) {
                return call_with_values(minim_car(args), minim_cadr(args), env);
            }

            return minim_prim_proc(proc)(args);
        } else if (minim_is_closure_proc(proc)) {
            minim_object *vars;

            // check arity and extend environment
            check_closure_proc_arity(proc, args);
            env = extend_env(minim_null, minim_null, minim_closure_env(proc));

            // process args
            vars = minim_closure_args(proc);
            while (minim_is_pair(vars)) {
                env_define_var_no_check(env, minim_car(vars), minim_car(args));
                vars = minim_cdr(vars);
                args = minim_cdr(args);
            }

            // optionally process rest argument
            if (minim_is_symbol(vars))
                env_define_var_no_check(env, vars, copy_list(args));

            // tail call
            expr = minim_closure_body(proc);
            goto loop;
        } else {
            fprintf(stderr, "not a procedure\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "bad syntax\n");
        exit(1);
    }

    fprintf(stderr, "unreachable\n");
    exit(1);
}

//
//  Interpreter initialization
//

#define add_procedure(name, c_fn, min_arity, max_arity) {   \
    minim_object *sym = intern(name);                       \
    env_define_var(env, sym,                                \
                   make_prim_proc(c_fn, minim_symbol(sym),  \
                                  min_arity, max_arity));   \
}

void populate_env(minim_object *env) {
    add_procedure("null?", is_null_proc, 1, 1);
    add_procedure("void?", is_void_proc, 1, 1);
    add_procedure("eof-object?", is_eof_proc, 1, 1);
    add_procedure("boolean?", is_bool_proc, 1, 1);
    add_procedure("symbol?", is_symbol_proc, 1, 1);
    add_procedure("integer?", is_fixnum_proc, 1, 1);
    add_procedure("char?", is_char_proc, 1, 1);
    add_procedure("string?", is_string_proc, 1, 1);
    add_procedure("pair?", is_pair_proc, 1, 1);
    add_procedure("list?", is_list_proc, 1, 1);
    add_procedure("procedure?", is_procedure_proc, 1, 1);
    add_procedure("input-port?", is_input_port_proc, 1, 1);
    add_procedure("output-port?", is_output_port_proc, 1, 1);

    add_procedure("char->integer", char_to_integer_proc, 1, 1);
    add_procedure("integer->char", integer_to_char_proc, 1, 1);
    add_procedure("number->string", number_to_string_proc, 1, 1);
    add_procedure("string->number", string_to_number_proc, 1, 1);
    add_procedure("symbol->string", symbol_to_string_proc, 1, 1);
    add_procedure("string->symbol", string_to_symbol_proc, 1, 1);

    add_procedure("eq?", eq_proc, 2, 2);
    add_procedure("equal?", equal_proc, 2, 2);

    add_procedure("not", not_proc, 1, 1);

    add_procedure("cons", cons_proc, 2, 2);
    add_procedure("car", car_proc, 1, 1);
    add_procedure("cdr", cdr_proc, 1, 1);
    add_procedure("set-car!", set_car_proc, 2, 2);
    add_procedure("set-cdr!", set_cdr_proc, 2, 2);

    add_procedure("list", list_proc, 0, ARG_MAX);
    add_procedure("andmap", andmap_proc, 2, 2);
    add_procedure("ormap", ormap_proc, 2, 2);

    add_procedure("+", add_proc, 0, ARG_MAX);
    add_procedure("-", sub_proc, 1, ARG_MAX);
    add_procedure("*", mul_proc, 0, ARG_MAX);
    add_procedure("/", div_proc, 1, ARG_MAX);
    add_procedure("remainder", remainder_proc, 2, 2);
    add_procedure("modulo", modulo_proc, 2, 2);

    add_procedure("=", number_eq_proc, 2, 2);
    add_procedure(">=", number_ge_proc, 2, 2);
    add_procedure("<=", number_le_proc, 2, 2);
    add_procedure(">", number_gt_proc, 2, 2);
    add_procedure("<", number_lt_proc, 2, 2);

    add_procedure("make-string", make_string_proc, 1, 2);
    add_procedure("string", string_proc, 0, ARG_MAX);
    add_procedure("string-length", string_length_proc, 1, 1);
    add_procedure("string-ref", string_ref_proc, 2, 2);
    add_procedure("string-set!", string_set_proc, 3, 3);
    
    add_procedure("syntax?", is_syntax_proc, 1, 1);
    add_procedure("syntax-e", syntax_e_proc, 1, 1);
    add_procedure("syntax-loc", syntax_loc_proc, 2, 2);
    add_procedure("datum->syntax", to_syntax_proc, 1, 1);

    add_procedure("interaction-environment", interaction_environment_proc, 0, 0);
    add_procedure("null-environment", empty_environment_proc, 0, 0);
    add_procedure("environment", environment_proc, 0, 0);
    add_procedure("current-environment", current_environment_proc, 0, 0);

    add_procedure("extend-environment", extend_environment_proc, 1, 1);
    add_procedure("environment-variable-value", environment_variable_value_proc, 2, 3);
    add_procedure("environment-set-variable-value!", environment_set_variable_value_proc, 3, 3);

    add_procedure("eval", eval_proc, 1, 2);
    add_procedure("apply", apply_proc, 2, ARG_MAX);
    add_procedure("void", void_proc, 0, 0);

    add_procedure("call-with-values", call_with_values_proc, 2, 2);
    add_procedure("values", values_proc, 0, ARG_MAX);

    add_procedure("current-input-port", current_input_port_proc, 0, 0);
    add_procedure("current-output-port", current_output_port_proc, 0, 0);
    add_procedure("open-input-file", open_input_port_proc, 1, 1);
    add_procedure("open-output-file", open_output_port_proc, 1, 1);
    add_procedure("close-input-port", close_input_port_proc, 1, 1);
    add_procedure("close-output-port", close_output_port_proc, 1, 1);
    add_procedure("read", read_proc, 0, 1);
    add_procedure("read-char", read_char_proc, 0, 1);
    add_procedure("peek-char", peek_char_proc, 0, 1);
    add_procedure("char-ready?", char_is_ready_proc, 0, 1);
    add_procedure("display", display_proc, 1, 2);
    add_procedure("write", write_proc, 1, 2);
    add_procedure("write-char", write_char_proc, 1, 2);
    add_procedure("newline", newline_proc, 0, 1);
    
    add_procedure("load", load_proc, 1, 1);
    add_procedure("error", error_proc, 1, ARG_MAX);
    add_procedure("exit", exit_proc, 0, 0);
    add_procedure("syntax-error", syntax_error_proc, 2, 4);
    add_procedure("current-directory", current_directory_proc, 0, 1);
    add_procedure("boot-expander?", boot_expander_proc, 0, 1);
}

minim_object *make_env() {
    minim_object *env;

    env = setup_env();
    populate_env(env);
    return env;
}

void minim_boot_init() {
    minim_thread *th;

    GC_pause();

    // initialize globals
    globals = GC_alloc(sizeof(minim_globals));
    globals->symbols = make_intern_table();
    globals->current_thread = GC_alloc(sizeof(minim_thread));

    minim_null = GC_alloc(sizeof(minim_object));
    minim_true = GC_alloc(sizeof(minim_object));
    minim_false = GC_alloc(sizeof(minim_object));
    minim_eof = GC_alloc(sizeof(minim_object));
    minim_void = GC_alloc(sizeof(minim_object));
    minim_values = GC_alloc(sizeof(minim_object));

    minim_null->type = MINIM_NULL_TYPE;
    minim_true->type = MINIM_TRUE_TYPE;
    minim_false->type = MINIM_FALSE_TYPE;
    minim_eof->type = MINIM_EOF_TYPE;
    minim_void->type = MINIM_VOID_TYPE;
    minim_values->type = MINIM_VALUES_TYPE;

    quote_symbol = intern("quote");
    define_symbol = intern("define");
    define_values_symbol = intern("define-values");
    let_symbol = intern("let");
    let_values_symbol = intern("let-values");
    letrec_symbol = intern("letrec");
    letrec_values_symbol = intern("letrec-values");
    setb_symbol = intern("set!");
    if_symbol = intern("if");
    lambda_symbol = intern("lambda");
    begin_symbol = intern("begin");
    cond_symbol = intern("cond");
    else_symbol = intern("else");
    and_symbol = intern("and");
    or_symbol = intern("or");
    syntax_symbol = intern("syntax");
    syntax_loc_symbol = intern("syntax/loc");
    quote_syntax_symbol = intern("quote-syntax");

    empty_env = minim_null;

    // initialize thread

    th = current_thread();
    global_env(th) = make_env();
    input_port(th) = make_input_port(stdin);
    output_port(th) = make_output_port(stdout);
    current_directory(th) = make_string2(get_current_dir());
    boot_expander(th) = minim_true;
    values_buffer(th) = GC_alloc(INIT_VALUES_BUFFER_LEN * sizeof(minim_object*));
    values_buffer_size(th) = INIT_VALUES_BUFFER_LEN;
    values_buffer_count(th) = 0;
    th->pid = 0;    // TODO

    // GC roots

    GC_register_root(minim_null);
    GC_register_root(minim_true);
    GC_register_root(minim_false);
    GC_register_root(minim_eof);
    GC_register_root(minim_void);
    GC_register_root(minim_values);

    GC_register_root(globals);

    GC_resume();
}

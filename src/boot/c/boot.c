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

minim_object *quote_symbol;
minim_object *define_symbol;
minim_object *setb_symbol;
minim_object *if_symbol;
minim_object *lambda_symbol;
minim_object *begin_symbol;
minim_object *cond_symbol;
minim_object *else_symbol;
minim_object *let_symbol;
minim_object *and_symbol;
minim_object *or_symbol;
minim_object *syntax_symbol;
minim_object *syntax_loc_symbol;
minim_object *quote_syntax_symbol;

minim_object *empty_env;
minim_object *global_env;

minim_object *input_port;
minim_object *output_port;

intern_table *symbols;


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
//  Extra functions
//

// Makes an association list.
// Unsafe: only iterates on `xs`.
minim_object *make_assoc(minim_object *xs, minim_object *ys) {
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
minim_object *copy_list(minim_object *xs) {
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
    return intern_symbol(symbols, minim_string(minim_car(args)));
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

minim_object *string_proc(minim_object *args) {
    minim_string_object *o;
    long len, i;
    char *str;

    len = list_length(args);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (i = 0; i < len; ++i) {
        str[i] = minim_char(minim_car(args));
        args = minim_cdr(args);
    }

    o = GC_alloc(sizeof(minim_string_object));
    o->type = MINIM_STRING_TYPE;
    o->value = str;
    return ((minim_object *) o);
}

minim_object *syntax_e_proc(minim_object *args) {
    return minim_syntax_e(minim_car(args));
}

minim_object *syntax_loc_proc(minim_object *args) {
    return minim_syntax_loc(minim_car(args));
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
    return global_env;
}

minim_object *empty_environment_proc(minim_object *args) {
    return setup_env();
}

minim_object *environment_proc(minim_object *args) {
    return make_env();
}

minim_object *current_input_port_proc(minim_object *args) {
    return input_port;
}

minim_object *current_output_port_proc(minim_object *args) {
    return output_port;
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
    FILE *in_p = minim_port(minim_is_null(args) ? input_port : minim_car(args));
    minim_object *o = read_object(in_p);
    return (o == NULL) ? minim_eof : o;
}

minim_object *read_char_proc(minim_object *args) {
    FILE *in_p = minim_port(minim_is_null(args) ? input_port : minim_car(args));
    return make_char(getc(in_p));
}

minim_object *peek_char_proc(minim_object *args) {
    FILE *in_p = minim_port(minim_is_null(args) ? input_port : minim_car(args));
    int ch = getc(in_p);
    ungetc(ch, in_p);
    return make_char(ch);
}

minim_object *char_is_ready_proc(minim_object *args) {
    FILE *in_p = minim_port(minim_is_null(args) ? input_port : minim_car(args));
    int ch = getc(in_p);
    ungetc(ch, in_p);
    return (ch == EOF) ? minim_false : minim_true;
}

minim_object *write_proc(minim_object *args) {
    FILE *out_p;
    minim_object *o;

    o = minim_car(args);
    args = minim_cdr(args);
    out_p = minim_port(minim_is_null(args) ? output_port : minim_car(args));

    write_object(out_p, o);
    return minim_void;
}

minim_object *write_char_proc(minim_object *args) {
    FILE *out_p;
    minim_object *ch;

    ch = minim_car(args);
    args = minim_cdr(args);
    out_p = minim_port(minim_is_null(args) ? output_port : minim_car(args));

    putc(minim_char(ch), out_p);
    return minim_void;
}

minim_object *load_proc(minim_object *args) {
    FILE *stream;
    minim_object *expr, *result;
    char *fname, *old_cwd;
    
    fname = minim_string(minim_car(args));
    stream = fopen(fname, "r");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    old_cwd = get_current_dir();
    set_current_dir(get_file_path(fname));

    result = minim_void;
    while ((expr = read_object(stream)) != NULL)
        result = eval_expr(expr, global_env);

    set_current_dir(old_cwd);

    fclose(stream);
    return result;
}

minim_object *error_proc(minim_object *args) {
    while (!minim_is_null(args)) {
        write_object(stderr, minim_car(args));
        fprintf(stderr, " ");
        args = minim_cdr(args);
    }

    printf("\n");
    exit(1);
}

//
//  Runtime Error / Assertions
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

static void check_proc_arity(proc_arity *arity, minim_object *args, const char *name) {
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
}

#define check_prim_proc_arity(prim, args)   \
    check_proc_arity(&minim_prim_arity(prim), args, minim_prim_proc_name(prim))

#define check_closure_proc_arity(prim, args)    \
    check_proc_arity(&minim_closure_arity(prim), args, minim_closure_name(prim))

//
//  Syntax check
//

static void bad_syntax_exn(minim_object *expr) {
    fprintf(stderr, "%s: bad syntax\n", minim_symbol(minim_car(expr)));
    fprintf(stderr, " at: ");
    write_object2(stderr, expr, 1);
    fputc('\n', stderr);
    exit(1);
}

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
    minim_object *rest = minim_cdr(expr);
    if (!minim_is_pair(rest))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
        bad_syntax_exn(expr);
}

// Already assumes `expr` is `(<name> . <???>)`
// Check: `expr` must be `(<name> <symbol> <datum>)
static void check_define(minim_object *expr) {
    minim_object *rest;
    
    rest = minim_cdr(expr);
    if (!minim_is_pair(rest) || !minim_is_symbol(minim_car(rest)))
        bad_syntax_exn(expr);

    rest = minim_cdr(rest);
    if (!minim_is_pair(rest) || !minim_is_null(minim_cdr(rest)))
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

static int is_pair_starting_with(minim_object *maybe, minim_object *key) {
    return minim_is_pair(maybe) && minim_is_symbol(minim_car(maybe)) && minim_car(maybe) == key;
}

static int is_quoted(minim_object *expr) {
    return is_pair_starting_with(expr, quote_symbol);
}

static int is_syntax(minim_object *expr) {
    return is_pair_starting_with(expr, syntax_symbol);
}

static int is_syntax_loc(minim_object *expr) {
    return is_pair_starting_with(expr, syntax_loc_symbol);
}

static int is_quoted_syntax(minim_object *expr) {
    return is_pair_starting_with(expr, quote_syntax_symbol);
}

static int is_assignment(minim_object *expr) {
    return is_pair_starting_with(expr, setb_symbol);
}

static int is_definition(minim_object *expr) {
    return is_pair_starting_with(expr, define_symbol);
}

static int is_let(minim_object *expr) {
    return is_pair_starting_with(expr, let_symbol);
}

static int is_if(minim_object *expr) {
    return is_pair_starting_with(expr, if_symbol);
}

static int is_cond(minim_object *expr) {
    return is_pair_starting_with(expr, cond_symbol);
}

static int is_lambda(minim_object *expr) {
    return is_pair_starting_with(expr, lambda_symbol);
}

static int is_begin(minim_object *expr) {
    return is_pair_starting_with(expr, begin_symbol);
}

static int is_and(minim_object *expr) {
    return is_pair_starting_with(expr, and_symbol);
}

static int is_or(minim_object *expr) {
    return is_pair_starting_with(expr, or_symbol);
}

static minim_object *let_vars(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_caar(bindings), let_vars(minim_cdr(bindings))));
}

static minim_object *let_vals(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_car(minim_cdar(bindings)), let_vals(minim_cdr(bindings))));
}

static minim_object *apply_args(minim_object *args) {
    if (minim_is_null(args)) {
        return minim_null;
    } else if (minim_is_null(minim_cdr(args))) {
        return minim_car(args);
    } else {
        return make_pair(minim_car(args), apply_args(minim_cdr(args)));
    }
}

static minim_object *eval_exprs(minim_object *exprs, minim_object *env) {
    if (minim_is_null(exprs)) {
        return minim_null;
    } else {
        return make_pair(eval_expr(minim_car(exprs), env),
                         eval_exprs(minim_cdr(exprs), env));
    }
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {
    minim_object *proc, *args, *result;
    short min_arity, max_arity;

    // write_object(stdout, expr);
    // printf("\n");

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
    } else if (is_quoted(expr)) {
        check_1ary_syntax(expr);
        return minim_cadr(expr);
    } else if (is_syntax(expr) || is_quoted_syntax(expr)) {
        check_1ary_syntax(expr);
        return make_syntax(minim_cadr(expr), minim_false);
    } else if (is_syntax_loc(expr)) {
        check_2ary_syntax(expr);
        return make_syntax(minim_car(minim_cddr(expr)), minim_cadr(expr));
    } else if (is_assignment(expr)) {
        check_define(expr);
        env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
        return minim_void;
    } else if (is_definition(expr)) {
        check_define(expr);
        env_define_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
        return minim_void;
    } else if (is_let(expr)) {
        check_let(expr);
        args = eval_exprs(let_vals(minim_cadr(expr)), env);
        env = extend_env(let_vars(minim_cadr(expr)), args, env);
        expr = make_pair(begin_symbol, (minim_cddr(expr)));
        goto loop;
    } else if (is_if(expr)) {
        if (!minim_is_false(eval_expr(minim_cadr(expr), env)))
            expr = minim_car(minim_cddr(expr));
        else
            expr = minim_cadr(minim_cddr(expr));

        goto loop;
    } else if (is_cond(expr)) {
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
    } else if (is_lambda(expr)) {
        check_lambda(expr, &min_arity, &max_arity);
        return make_closure_proc(minim_cadr(expr),
                                 make_pair(begin_symbol, minim_cddr(expr)),
                                 env,
                                 min_arity,
                                 max_arity);
    } else if (is_begin(expr)) {
        minim_object *old_expr = expr;
        expr = minim_cdr(expr);
        if (minim_is_null(expr))
            return minim_void;

        while (minim_is_pair(expr) && !minim_is_null(minim_cdr(expr))) {
            eval_expr(minim_car(expr), env);
            expr = minim_cdr(expr);
        }

        if (!minim_is_pair(expr) || !minim_is_null(minim_cdr(expr)))
            bad_syntax_exn(old_expr);

        expr = minim_car(expr);
        goto loop;
    } else if (is_and(expr)) {
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
    } else if (is_or(expr)) {
        expr = minim_cdr(expr);
        if (minim_is_null(expr))
            return minim_false;

        while (!minim_is_null(minim_cdr(expr))) {
            result = eval_expr(minim_car(expr), env);
            if (minim_is_true(result))
                return result;
            expr = minim_cdr(expr);
        }
        
        expr = minim_car(expr);
        goto loop;
    } else if (minim_is_pair(expr)) {
        proc = eval_expr(minim_car(expr), env);
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
                args = minim_cdr(vars);
            }

            // optionally process rest argument
            if (minim_is_symbol(vars))
                env_define_var(env, vars, copy_list(args));

            // tail call
            expr = minim_closure_body(proc);
            goto loop;
        } else {
            fprintf(stderr, "not a procedure\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "cannot evaluate expresion\n");
        exit(1);
    }

    fprintf(stderr, "unreachable\n");
    exit(1);
}

//
//  Interpreter initialization
//

#define add_procedure(name, c_fn, min_arity, max_arity) {               \
    minim_object *sym = intern_symbol(symbols, name);                   \
    env_define_var(env, sym, make_prim_proc(c_fn, minim_symbol(sym),    \
                                            min_arity, max_arity));     \
}

void populate_env(minim_object *env) {
    add_procedure("null?", is_null_proc, 1, 1);
    add_procedure("eof-object?", is_eof_proc, 1, 1);
    add_procedure("boolean?", is_bool_proc, 1, 1);
    add_procedure("symbol?", is_symbol_proc, 1, 1);
    add_procedure("integer?", is_fixnum_proc, 1, 1);
    add_procedure("char?", is_char_proc, 1, 1);
    add_procedure("string?", is_string_proc, 1, 1);
    add_procedure("pair?", is_pair_proc, 1, 1);
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

    add_procedure("string", string_proc, 0, ARG_MAX);
    
    add_procedure("syntax-e", syntax_e_proc, 1, 1);
    add_procedure("syntax-loc", syntax_loc_proc, 2, 2);

    add_procedure("interaction-environment", interaction_environment_proc, 0, 0);
    add_procedure("null-environment", empty_environment_proc, 0, 0);
    add_procedure("environment", environment_proc, 0, 0);

    add_procedure("eval", eval_proc, 1, 2);
    add_procedure("apply", apply_proc, 1, ARG_MAX);
    add_procedure("void", void_proc, 0, 0);

    add_procedure("current-input-port", current_input_port_proc, 0, 0);
    add_procedure("current-output-port", current_output_port_proc, 0, 0);
    add_procedure("open-input-file", open_input_port_proc, 1, 1);
    add_procedure("open-output-file", open_output_port_proc, 1, 1);
    add_procedure("close-input-port", close_input_port_proc, 1, 1);
    add_procedure("close-output-port", close_output_port_proc, 1, 1);
    add_procedure("read", read_proc, 1, 2);
    add_procedure("read-char", read_char_proc, 0, 1);
    add_procedure("peek-char", peek_char_proc, 0, 1);
    add_procedure("char-ready?", char_is_ready_proc, 0, 1);
    add_procedure("write", write_proc, 1, 2);
    add_procedure("write-char", write_char_proc, 1, 2);
    
    add_procedure("load", load_proc, 1, 1);
    add_procedure("error", error_proc, 1, ARG_MAX);
}

minim_object *make_env() {
    minim_object *env;

    env = setup_env();
    populate_env(env);
    return env;
}

void minim_boot_init() {
    symbols = make_intern_table();

    minim_null = GC_alloc(sizeof(minim_object));
    minim_true = GC_alloc(sizeof(minim_object));
    minim_false = GC_alloc(sizeof(minim_object));
    minim_eof = GC_alloc(sizeof(minim_object));
    minim_void = GC_alloc(sizeof(minim_object));

    minim_null->type = MINIM_NULL_TYPE;
    minim_true->type = MINIM_TRUE_TYPE;
    minim_false->type = MINIM_FALSE_TYPE;
    minim_eof->type = MINIM_EOF_TYPE;
    minim_void->type = MINIM_VOID_TYPE;

    quote_symbol = intern_symbol(symbols, "quote");
    define_symbol = intern_symbol(symbols, "define");
    setb_symbol = intern_symbol(symbols, "set!");
    if_symbol = intern_symbol(symbols, "if");
    lambda_symbol = intern_symbol(symbols, "lambda");
    begin_symbol = intern_symbol(symbols, "begin");
    cond_symbol = intern_symbol(symbols, "cond");
    else_symbol = intern_symbol(symbols, "else");
    let_symbol = intern_symbol(symbols, "let");
    and_symbol = intern_symbol(symbols, "and");
    or_symbol = intern_symbol(symbols, "or");
    syntax_symbol = intern_symbol(symbols, "syntax");
    syntax_loc_symbol = intern_symbol(symbols, "syntax/loc");
    quote_syntax_symbol = intern_symbol(symbols, "quote-syntax");

    empty_env = minim_null;
    global_env = make_env();
    input_port = make_input_port(stdin);
    output_port = make_output_port(stdout);
}

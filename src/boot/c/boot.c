/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "boot.h"

//
//  Constructors
//

minim_object *make_char(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
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

//
//  Primitive library
//

minim_object *is_symbol_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_symbol(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_char_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_char(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_input_port_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_input_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_output_port_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_output_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_syntax_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_syntax(minim_car(args)) ? minim_true : minim_false;
}

minim_object *char_to_integer_proc(minim_object *args) {
    // (-> char integer)
    minim_object *o = minim_car(args);
    if (!minim_is_char(o))
        bad_type_exn("char->integer", "char?", o);
    return make_fixnum(minim_char(o));
}

minim_object *integer_to_char_proc(minim_object *args) {
    // (-> integer char)
    minim_object *o = minim_car(args);
    if (!minim_is_fixnum(o))
        bad_type_exn("integer->char", "integer?", o);
    return make_char(minim_fixnum(minim_car(args)));
}

minim_object *syntax_e_proc(minim_object *args) {
    // (-> syntax any)
    if (!minim_is_syntax(minim_car(args)))
        bad_type_exn("syntax-e", "syntax?", minim_car(args));
    return minim_syntax_e(minim_car(args));
}

minim_object *syntax_loc_proc(minim_object *args) {
    // (-> syntax any)
    if (!minim_is_syntax(minim_car(args)))
        bad_type_exn("syntax-loc", "syntax?", minim_car(args));
    return minim_syntax_loc(minim_car(args));
}

minim_object *to_syntax_proc(minim_object *args) {
    // (-> any syntax)
    return to_syntax(minim_car(args));
}

minim_object *current_input_port_proc(minim_object *args) {
    // (-> input-port)
    return input_port(current_thread());
}

minim_object *current_output_port_proc(minim_object *args) {
    // (-> output-port)
    return output_port(current_thread());
}

minim_object *open_input_port_proc(minim_object *args) {
    // (-> string input-port)
    minim_object *port;
    FILE *stream;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("open-input-port", "string?", minim_car(args));

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
    // (-> string output-port)
    minim_object *port;
    FILE *stream;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("open-output-port", "string?", minim_car(args));
    
    stream = fopen(minim_string(minim_car(args)), "w");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    port = make_output_port(stream);
    minim_port_set(port, MINIM_PORT_OPEN);
    return port;
}

minim_object *close_input_port_proc(minim_object *args) {
    // (-> input-port)
    if (!minim_is_input_port(minim_car(args)))
        bad_type_exn("close-input-port", "input-port?", minim_car(args));

    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *close_output_port_proc(minim_object *args) {
    // (-> output-port)
    if (!minim_is_output_port(minim_car(args)))
        bad_type_exn("close-output-port", "output-port?", minim_car(args));

    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *read_proc(minim_object *args) {
    // (-> any)
    // (-> input-port any)
    minim_object *in_p, *o;

    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("read", "input-port?", in_p);
    }

    o = read_object(minim_port(in_p));
    return (o == NULL) ? minim_eof : o;
}

minim_object *read_char_proc(minim_object *args) {
    // (-> char)
    // (-> input-port char)
    minim_object *in_p;
    int ch;
    
    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("read-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *peek_char_proc(minim_object *args) {
    // (-> char)
    // (-> input-port char)
    minim_object *in_p;
    int ch;
    
    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *char_is_ready_proc(minim_object *args) {
    // (-> boolean)
    // (-> input-port boolean)
    minim_object *in_p;
    int ch;

    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_false : minim_true;
}

minim_object *display_proc(minim_object *args) {
    // (-> any void)
    // (-> any output-port void)
    minim_object *out_p, *o;

    o = minim_car(args);
    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 0, 1);
    return minim_void;
}

minim_object *write_proc(minim_object *args) {
    // (-> any void)
    // (-> any output-port void)
    minim_object *out_p, *o;

    o = minim_car(args);
    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object(minim_port(out_p), o);
    return minim_void;
}

minim_object *write_char_proc(minim_object *args) {
    // (-> char void)
    // (-> char output-port void)
    minim_object *out_p, *ch;

    ch = minim_car(args);
    if (!minim_is_char(ch))
        bad_type_exn("write-char", "char?", ch);

    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    putc(minim_char(ch), minim_port(out_p));
    return minim_void;
}

minim_object *newline_proc(minim_object *args) {
    // (-> void)
    // (-> output-port void)
    minim_object *out_p;

    if (minim_is_null(args)) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_car(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    putc('\n', minim_port(out_p));
    return minim_void;
}

minim_object *load_proc(minim_object *args) {
    // (-> string any)
    if (!minim_is_string(minim_car(args)))
        bad_type_exn("load", "string?", minim_car(args));
    return load_file(minim_string(minim_car(args)));
}

minim_object *error_proc(minim_object *args) {
    // (-> (or #f string symbol)
    //     string?
    //     any ...
    //     any)
    minim_object *who, *message, *reasons;

    who = minim_car(args);
    if (!minim_is_false(who) && !minim_is_symbol(who) && !minim_is_string(who))
        bad_type_exn("error", "(or #f string? symbol?)", who);

    message = minim_cadr(args);
    if (!minim_is_string(message))
        bad_type_exn("error", "string?", message);
    
    if (minim_is_false(who))
        printf("error");
    else
        write_object2(stderr, who, 1, 1);

    fprintf(stderr, ": ");
    write_object2(stderr, message, 1, 1);
    fprintf(stderr, "\n");

    reasons = minim_cddr(args);
    while (!minim_is_null(reasons)) {
        fprintf(stderr, " ");
        write_object2(stderr, minim_car(reasons), 1, 1);
        fprintf(stderr, "\n");
        reasons = minim_cdr(reasons);
    }

    exit(1);
}

minim_object *exit_proc(minim_object *args) {
    // (-> any)
    exit(0);
}

minim_object *syntax_error_proc(minim_object *args) {
    // (-> symbol string any)
    // (-> symbol string syntax any)
    // (-> symbol string syntax syntaxs any)
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
        if (!minim_is_syntax(where))
            bad_type_exn("syntax-error", "syntax?", where);

        if (!minim_is_null(minim_cdr(minim_cddr(args)))) {
            sub = minim_cadr(minim_cddr(args));
            if (!minim_is_syntax(sub))
                bad_type_exn("syntax-error", "syntax?", sub);

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
    // (-> string)
    // (-> string void)
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
    // (-> boolean)
    // (-> boolean void)
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
//  Initialization
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
    add_procedure("length", length_proc, 1, 1);
    add_procedure("reverse", reverse_proc, 1, 1);
    add_procedure("append", append_proc, 0, ARG_MAX);
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
    add_procedure("error", error_proc, 2, ARG_MAX);
    add_procedure("exit", exit_proc, 0, 0);
    add_procedure("syntax-error", syntax_error_proc, 2, 4);
    add_procedure("current-directory", current_directory_proc, 0, 1);
    add_procedure("boot-expander?", boot_expander_proc, 0, 1);
}

minim_object *make_env() {
    minim_object *env = setup_env();
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

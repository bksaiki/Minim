/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

//
//  Primitives
//

static minim_object *boot_expander_proc(int argc, minim_object **args) {
    // (-> boolean)
    // (-> boolean void)
    minim_object *val;
    minim_thread *th;

    th = current_thread();
    if (argc == 0) {
        // getter
        return boot_expander(th);
    } else {
        // setter
        val = args[0];
        if (!minim_is_bool(val))
            bad_type_exn("boot-expander?", "boolean?", val);
        boot_expander(th) = val;
        return minim_void;
    }
}

//
//  Initialization
//

#define add_value(name, c_val)                  \
    env_define_var(env, intern(name), c_val);

#define add_procedure(name, c_fn, min_arity, max_arity) {           \
    minim_object *sym = intern(name);                               \
    env_define_var(env, sym,                               \
                   make_prim_proc(c_fn, minim_symbol(sym), \
                                  min_arity, max_arity));  \
}

void populate_env(minim_object *env) {
    add_value("null", minim_null);
    add_value("true", minim_true);
    add_value("false", minim_false);
    add_value("eof", minim_eof);

    add_procedure("null?", is_null_proc, 1, 1);
    add_procedure("void?", is_void_proc, 1, 1);
    add_procedure("eof-object?", is_eof_proc, 1, 1);
    add_procedure("boolean?", is_bool_proc, 1, 1);
    add_procedure("symbol?", is_symbol_proc, 1, 1);
    add_procedure("integer?", is_fixnum_proc, 1, 1);
    add_procedure("number?", is_fixnum_proc, 1, 1);
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
    add_procedure("caar", caar_proc, 1, 1);
    add_procedure("cadr", cadr_proc, 1, 1);
    add_procedure("cdar", cdar_proc, 1, 1);
    add_procedure("cddr", cddr_proc, 1, 1);
    add_procedure("caaar", caaar_proc, 1, 1);
    add_procedure("caadr", caadr_proc, 1, 1);
    add_procedure("cadar", cadar_proc, 1, 1);
    add_procedure("caddr", caddr_proc, 1, 1);
    add_procedure("cdaar", cdaar_proc, 1, 1);
    add_procedure("cdadr", cdadr_proc, 1, 1);
    add_procedure("cddar", cddar_proc, 1, 1);
    add_procedure("cdddr", cdddr_proc, 1, 1);
    add_procedure("caaaar", caaaar_proc, 1, 1);
    add_procedure("caaadr", caaadr_proc, 1, 1);
    add_procedure("caadar", caadar_proc, 1, 1);
    add_procedure("caaddr", caaddr_proc, 1, 1);
    add_procedure("cadaar", cadaar_proc, 1, 1);
    add_procedure("cadadr", cadadr_proc, 1, 1);
    add_procedure("caddar", caddar_proc, 1, 1);
    add_procedure("cadddr", cadddr_proc, 1, 1);
    add_procedure("cdaaar", cdaaar_proc, 1, 1);
    add_procedure("cdaadr", cdaadr_proc, 1, 1);
    add_procedure("cdadar", cdadar_proc, 1, 1);
    add_procedure("cdaddr", cdaddr_proc, 1, 1);
    add_procedure("cddaar", cddaar_proc, 1, 1);
    add_procedure("cddadr", cddadr_proc, 1, 1);
    add_procedure("cdddar", cdddar_proc, 1, 1);
    add_procedure("cddddr", cddddr_proc, 1, 1);
    add_procedure("set-car!", set_car_proc, 2, 2);
    add_procedure("set-cdr!", set_cdr_proc, 2, 2);

    add_procedure("list", list_proc, 0, ARG_MAX);
    add_procedure("make-list", make_list_proc, 2, 2);
    add_procedure("length", length_proc, 1, 1);
    add_procedure("reverse", reverse_proc, 1, 1);
    add_procedure("append", append_proc, 0, ARG_MAX);
    add_procedure("for-each", for_each_proc, 2, ARG_MAX);
    add_procedure("map", map_proc, 2, ARG_MAX);
    add_procedure("andmap", andmap_proc, 2, 2);
    add_procedure("ormap", ormap_proc, 2, 2);

    add_procedure("vector?", is_vector_proc, 1, 1);
    add_procedure("make-vector", make_vector_proc, 1, 2);
    add_procedure("vector", vector_proc, 0, ARG_MAX);
    add_procedure("vector-length", vector_length_proc, 1, 1);
    add_procedure("vector-ref", vector_ref_proc, 2, 2);
    add_procedure("vector-set!", vector_set_proc, 3, 3);
    add_procedure("vector-fill!", vector_fill_proc, 2, 2);
    add_procedure("vector->list", vector_to_list_proc, 1, 1);
    add_procedure("list->vector", list_to_vector_proc, 1, 1)

    add_procedure("+", add_proc, 0, ARG_MAX);
    add_procedure("-", sub_proc, 1, ARG_MAX);
    add_procedure("*", mul_proc, 0, ARG_MAX);
    add_procedure("/", div_proc, 1, ARG_MAX);
    add_procedure("remainder", remainder_proc, 2, 2);
    add_procedure("modulo", modulo_proc, 2, 2);

    add_procedure("=", number_eq_proc, 2, ARG_MAX);
    add_procedure(">=", number_ge_proc, 2, ARG_MAX);
    add_procedure("<=", number_le_proc, 2, ARG_MAX);
    add_procedure(">", number_gt_proc, 2, ARG_MAX);
    add_procedure("<", number_lt_proc, 2, ARG_MAX);

    add_procedure("make-string", make_string_proc, 1, 2);
    add_procedure("string", string_proc, 0, ARG_MAX);
    add_procedure("string-length", string_length_proc, 1, 1);
    add_procedure("string-ref", string_ref_proc, 2, 2);
    add_procedure("string-set!", string_set_proc, 3, 3);
    add_procedure("string-append", string_append_proc, 0, ARG_MAX);

    add_procedure("hashtable?", is_hashtable_proc, 1, 1);
    add_procedure("make-eq-hashtable", make_eq_hashtable_proc, 0, 0);   // TODO: allow size hint?
    add_procedure("make-hashtable", make_hashtable_proc, 0, 0);         // TODO: allow size hint?
    add_procedure("hashtable-size", hashtable_size_proc, 1, 1);
    add_procedure("hashtable-contains?", hashtable_contains_proc, 2, 2);
    add_procedure("hashtable-set!", hashtable_set_proc, 3, 3);
    add_procedure("hashtable-delete!", hashtable_delete_proc, 2, 2);
    add_procedure("hashtable-update!", hashtable_update_proc, 3, 4);
    add_procedure("hashtable-ref", hashtable_ref_proc, 2, 3);
    add_procedure("hashtable-keys", hashtable_keys_proc, 1, 1);
    add_procedure("hashtable-copy", hashtable_copy_proc, 1, 1);     // TODO: set mutability
    add_procedure("hashtable-clear!", hashtable_clear_proc, 1, 1);

    add_procedure("eq-hash", eq_hash_proc, 1, 1);
    add_procedure("equal-hash", equal_hash_proc, 1, 1);
    
    add_procedure("syntax?", is_syntax_proc, 1, 1);
    add_procedure("syntax-e", syntax_e_proc, 1, 1);
    add_procedure("syntax-loc", syntax_loc_proc, 2, 2);
    add_procedure("datum->syntax", to_syntax_proc, 1, 1);
    add_procedure("syntax->datum", to_datum_proc, 1, 1);
    add_procedure("syntax->list", syntax_to_list_proc, 1, 1);

    add_procedure("pattern-variable?", is_pattern_var_proc, 1, 1);
    add_procedure("make-pattern-variable", make_pattern_var_proc, 2, 2);
    add_procedure("pattern-variable-value", pattern_var_value_proc, 1, 1);
    add_procedure("pattern-variable-depth", pattern_var_depth_proc, 1, 1);

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
    add_procedure("command-line", command_line_proc, 0, 0);
    add_procedure("boot-expander?", boot_expander_proc, 0, 1);

    add_procedure("install-literal-bundle!", install_literal_bundle_proc, 1, 1);
    add_procedure("install-procedure-bundle!", install_proc_bundle_proc, 1, 1);
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
    minim_empty_vec = make_vector(0, NULL);
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
    quote_syntax_symbol = intern("quote-syntax");

    empty_env = minim_null;

    // initialize thread

    th = current_thread();
    global_env(th) = make_env();
    input_port(th) = make_input_port(stdin);
    output_port(th) = make_output_port(stdout);
    current_directory(th) = make_string2(get_current_dir());
    command_line(th) = minim_null;
    boot_expander(th) = minim_true;
    values_buffer(th) = GC_alloc(INIT_VALUES_BUFFER_LEN * sizeof(minim_object*));
    values_buffer_size(th) = INIT_VALUES_BUFFER_LEN;
    values_buffer_count(th) = 0;
    th->pid = 0;    // TODO

    // GC roots

    GC_register_root(minim_null);
    GC_register_root(minim_empty_vec);
    GC_register_root(minim_true);
    GC_register_root(minim_false);
    GC_register_root(minim_eof);
    GC_register_root(minim_void);
    GC_register_root(minim_values);
    GC_register_root(globals);

    // Interpreter runtime

    irt_call_args = GC_alloc(CALL_ARGS_DEFAULT * sizeof(minim_object*));
    irt_saved_args = GC_alloc(SAVED_ARGS_DEFAULT * sizeof(minim_object*));
    irt_call_args_count = 0;
    irt_saved_args_count = 0;
    irt_call_args_size = CALL_ARGS_DEFAULT;
    irt_saved_args_size = SAVED_ARGS_DEFAULT;

    GC_resume();
}

/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

//
//  Initialization
//

#define add_value(name, c_val)                  \
    env_define_var(env, intern(name), c_val);

#define add_procedure(name, c_fn, min_arity, max_arity) { \
    mobj sym = intern(name); \
    env_define_var( \
        env, \
        sym, \
        Mprim( \
            c_fn, \
            minim_symbol(sym), \
            min_arity, \
            max_arity \
        ) \
    ); \
}

void populate_env(mobj env) {
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
    add_procedure("list?", minim_listp_proc, 1, 1);
    add_procedure("procedure?", is_procedure_proc, 1, 1);
    add_procedure("input-port?", is_input_port_proc, 1, 1);
    add_procedure("output-port?", is_output_port_proc, 1, 1);

    add_procedure("char->integer", char_to_integer_proc, 1, 1);
    add_procedure("integer->char", integer_to_char_proc, 1, 1);
    add_procedure("number->string", number_to_string_proc, 1, 1);
    add_procedure("string->number", string_to_number_proc, 1, 1);
    add_procedure("symbol->string", symbol_to_string_proc, 1, 1);
    add_procedure("string->symbol", string_to_symbol_proc, 1, 1);

    add_value("eq?", eq_proc_obj);
    add_value("equal?", equal_proc_obj);
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
    add_procedure("make-vector", Mvector_proc, 1, 2);
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

    add_procedure("make-string", Mstring_proc, 1, 2);
    add_procedure("string", string_proc, 0, ARG_MAX);
    add_procedure("string-length", string_length_proc, 1, 1);
    add_procedure("string-ref", string_ref_proc, 2, 2);
    add_procedure("string-set!", string_set_proc, 3, 3);
    add_procedure("string-append", string_append_proc, 0, ARG_MAX);
    add_procedure("format", format_proc, 1, ARG_MAX);

    add_procedure("record?", is_record_proc, 1, 1);
    add_procedure("record-type-descriptor?", is_record_rtd_proc, 1, 1);
    add_procedure("make-record-type-descriptor", make_rtd_proc, 6, 6);
    add_procedure("record-type-name", record_type_name_proc, 1, 1);
    add_procedure("record-type-parent", record_type_parent_proc, 1, 1);
    add_procedure("record-type-uid", record_type_uid_proc, 1, 1);
    add_procedure("record-type-opaque?", record_type_opaque_proc, 1, 1);
    add_procedure("record-type-sealed?", record_type_sealed_proc, 1, 1);
    add_procedure("record-type-field-names", record_type_fields_proc, 1, 1);
    add_procedure("record-type-field-mutable?", record_type_field_mutable_proc, 2, 2);
    add_procedure("$record-value?", is_record_value_proc, 1, 1);
    add_procedure("$make-record", Mrecord_proc, 1, ARG_MAX);
    add_procedure("$record-rtd", record_rtd_proc, 1, 1);
    add_procedure("$record-ref", record_ref_proc, 2, 2);
    add_procedure("$record-set!", record_set_proc, 3, 3);
    add_procedure("$current-record-equal-procedure", current_record_equal_procedure_proc, 0, 1);
    add_procedure("$current-record-hash-procedure", current_record_hash_procedure_proc, 0, 1);
    add_procedure("$current-record-write-procedure", current_record_write_procedure_proc, 0, 1);

    add_procedure("box?", is_box_proc, 1, 1);
    add_procedure("box", box_proc, 1, 1);
    add_procedure("unbox", unbox_proc, 1, 1);
    add_procedure("set-box!", box_set_proc, 2, 2);

    add_procedure("hashtable?", is_hashtable_proc, 1, 1);
    add_procedure("make-eq-hashtable", make_eq_hashtable_proc, 0, 0);   // TODO: allow size hint?
    add_procedure("make-hashtable", Mhashtable_proc, 0, 0);         // TODO: allow size hint?
    add_procedure("hashtable-size", hashtable_size_proc, 1, 1);
    add_procedure("hashtable-contains?", hashtable_contains_proc, 2, 2);
    add_procedure("hashtable-set!", hashtable_set_proc, 3, 3);
    add_procedure("hashtable-delete!", hashtable_delete_proc, 2, 2);
    add_procedure("hashtable-update!", hashtable_update_proc, 3, 4);
    add_procedure("hashtable-ref", hashtable_ref_proc, 2, 3);
    add_procedure("hashtable-keys", hashtable_keys_proc, 1, 1);
    add_procedure("hashtable-copy", hashtable_copy_proc, 1, 1);     // TODO: set mutability
    add_procedure("hashtable-clear!", hashtable_clear_proc, 1, 1);

    add_value("eq-hash", eq_hash_proc_obj);
    add_value("equal-hash", equal_hash_proc_obj);
    
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

    add_procedure("environment-names", environment_names_proc, 1, 1);
    add_procedure("extend-environment", extend_environment_proc, 1, 1);
    add_procedure("environment-variable-value", environment_variable_value_proc, 2, 3);
    add_procedure("environment-set-variable-value!", environment_set_variable_value_proc, 3, 3);

    add_procedure("procedure-arity", procedure_arity_proc, 1, 1);

    add_procedure("eval", eval_proc, 1, 2);
    add_procedure("apply", apply_proc, 2, ARG_MAX);
    add_procedure("identity", identity_proc, 1, 1);
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
    add_procedure("fprintf", fprintf_proc, 2, ARG_MAX);
    add_procedure("printf", printf_proc, 1, ARG_MAX);

    add_procedure("read-fasl", read_fasl_proc, 1, 1);
    add_procedure("write-fasl", write_fasl_proc, 1, 2);
    
    add_procedure("load", load_proc, 1, 1);
    add_procedure("error", error_proc, 2, ARG_MAX);
    add_procedure("exit", exit_proc, 0, 1);
    add_procedure("syntax-error", syntax_error_proc, 2, 4);
    add_procedure("current-directory", current_directory_proc, 0, 1);
    add_procedure("command-line", command_line_proc, 0, 0);
    add_procedure("version", version_proc, 0, 0);

    add_procedure("install-literal-bundle!", install_literal_bundle_proc, 1, 1);
    add_procedure("install-procedure-bundle!", install_proc_bundle_proc, 1, 1);
    add_procedure("reinstall-procedure-bundle!", reinstall_proc_bundle_proc, 2, 2);
    add_procedure("runtime-address", runtime_address_proc, 1, 1);
    add_procedure("enter-compiled!", enter_compiled_proc, 1, 1);
}

mobj make_env() {
    mobj env = setup_env();
    populate_env(env);
    return env;
}

void minim_boot_init() {
    minim_thread *th;
    mobj record_equal_proc_obj, record_hash_proc_obj, record_write_proc_obj;
    
    GC_pause();

    // initialize globals
    globals = GC_alloc(sizeof(minim_globals));
    globals->symbols = make_intern_table();
    globals->current_thread = GC_alloc(sizeof(minim_thread));

    // initialize special symbols

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

    // initialize special values

    minim_null = GC_alloc(sizeof(mobj));
    minim_true = GC_alloc(sizeof(mobj));
    minim_false = GC_alloc(sizeof(mobj));
    minim_eof = GC_alloc(sizeof(mobj));
    minim_void = GC_alloc(sizeof(mobj));
    minim_empty_vec = Mvector(0, NULL);
    minim_values = GC_alloc(sizeof(mobj));

    minim_type(minim_null) = MINIM_OBJ_SPECIAL;
    minim_type(minim_true) = MINIM_OBJ_SPECIAL;
    minim_type(minim_false) = MINIM_OBJ_SPECIAL;
    minim_type(minim_eof) = MINIM_OBJ_SPECIAL;
    minim_type(minim_void) = MINIM_OBJ_SPECIAL;
    minim_type(minim_values) = MINIM_OBJ_SPECIAL;

    empty_env = minim_null;

    minim_base_rtd = Mrecord(NULL, record_rtd_min_size);
    record_rtd_name(minim_base_rtd) = Mstring("$base-record-type");
    record_rtd_parent(minim_base_rtd) = minim_base_rtd;
    record_rtd_uid(minim_base_rtd) = minim_false;
    record_rtd_opaque(minim_base_rtd) = minim_true;
    record_rtd_sealed(minim_base_rtd) = minim_true;
    record_rtd_protocol(minim_base_rtd) = minim_false;

    eq_proc_obj = Mprim(eq_proc, "eq?", 2, 2);
    equal_proc_obj = Mprim(equal_proc, "equal?", 2, 2);
    eq_hash_proc_obj = Mprim(eq_hash_proc, "eq-hash", 1, 1);
    equal_hash_proc_obj = Mprim(equal_hash_proc, "equal-hash", 1, 1);

    record_equal_proc_obj = Mprim(default_record_equal_procedure_proc,
                                  "default-record-equal-procedure",
                                  3, 3);
    record_hash_proc_obj = Mprim(default_record_hash_procedure_proc,
                                 "default-record-hash-procedure",
                                 2, 2);
    record_write_proc_obj = Mprim(default_record_write_procedure_proc,
                                  "default-record-write-procedure",
                                  3, 3);

    // initialize thread

    th = current_thread();
    global_env(th) = make_env();
    input_port(th) = Minput_port(stdin);
    output_port(th) = Moutput_port(stdout);
    current_directory(th) = Mstring2(get_current_dir());
    command_line(th) = minim_null;
    record_equal_proc(th) = record_equal_proc_obj;
    record_hash_proc(th) = record_hash_proc_obj;
    record_write_proc(th) = record_write_proc_obj;

    values_buffer(th) = GC_alloc(INIT_VALUES_BUFFER_LEN * sizeof(mobj*));
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

    irt_call_args = GC_alloc(CALL_ARGS_DEFAULT * sizeof(mobj*));
    irt_saved_args = GC_alloc(SAVED_ARGS_DEFAULT * sizeof(mobj*));
    irt_call_args_count = 0;
    irt_saved_args_count = 0;
    irt_call_args_size = CALL_ARGS_DEFAULT;
    irt_saved_args_size = SAVED_ARGS_DEFAULT;

    GC_resume();
}

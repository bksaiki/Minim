// prim.c primitives

#include "../minim.h"

mobj Mprim(mprim_proc proc, char *name, short min_arity, short max_arity) {
    mobj o = GC_alloc(minim_prim_size);
    minim_type(o) = MINIM_OBJ_PRIM;
    minim_prim(o) = proc;
    minim_prim_argc_low(o) = min_arity;
    minim_prim_argc_high(o) = max_arity;
    minim_prim_name(o) = name;
    return o;
}

mobj Mprim2(void *fn, char *name, short arity) {
    mobj o = GC_alloc(minim_prim2_size);
    minim_type(o) = MINIM_OBJ_PRIM2;
    minim_prim2_argc(o) = arity;
    minim_prim2_proc(o) = fn;
    minim_prim2_name(o) = name;
    return o;
}

#define add_value(env, name, c_val)  \
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

#define add_unsafe_procedure(name, c_fn, arity) { \
    mobj sym = intern(name); \
    env_define_var(env, intern(name), Mprim2(c_fn, minim_symbol(sym), arity)); \
}

void init_prims(mobj env) {
    eq_proc_obj = Mprim2(eq_proc, "eq?", 2);
    equal_proc_obj = Mprim2(equal_proc, "equal?", 2);
    eq_hash_proc_obj = Mprim(eq_hash_proc, "eq-hash", 1, 1);
    equal_hash_proc_obj = Mprim(equal_hash_proc, "equal-hash", 1, 1);

    add_unsafe_procedure("null?", nullp_proc, 1);
    add_unsafe_procedure("void?", voidp_proc, 1);
    add_unsafe_procedure("eof-object?", eofp_proc, 1);
    add_unsafe_procedure("procedure?", procp_proc, 1);

    add_unsafe_procedure("boolean?", boolp_proc, 1);
    add_unsafe_procedure("not", not_proc, 1);

    add_unsafe_procedure("char?", charp_proc, 1);
    add_unsafe_procedure("$char->integer", char_to_integer, 1);
    add_unsafe_procedure("$integer->char", integer_to_char, 1);

    add_unsafe_procedure("symbol?", symbolp_proc, 1);
    add_procedure("symbol->string", symbol_to_string_proc, 1, 1);
    add_procedure("string->symbol", string_to_symbol_proc, 1, 1);

    add_unsafe_procedure("pair?", consp_proc, 1);
    add_unsafe_procedure("list?", listp_proc, 1);
    add_unsafe_procedure("cons", Mcons, 2);
    add_unsafe_procedure("$car", car_proc, 1);
    add_unsafe_procedure("$cdr", cdr_proc, 1);
    add_unsafe_procedure("$caar", caar_proc, 1);
    add_unsafe_procedure("$cadr", cadr_proc, 1);
    add_unsafe_procedure("$cdar", cdar_proc, 1);
    add_unsafe_procedure("$cddr", cddr_proc, 1);
    add_unsafe_procedure("$caaar", caaar_proc, 1);
    add_unsafe_procedure("$caadr", caadr_proc, 1);
    add_unsafe_procedure("$cadar", cadar_proc, 1);
    add_unsafe_procedure("$caddr", caddr_proc, 1);
    add_unsafe_procedure("$cdaar", cdaar_proc, 1);
    add_unsafe_procedure("$cdadr", cdadr_proc, 1);
    add_unsafe_procedure("$cddar", cddar_proc, 1);
    add_unsafe_procedure("$cdddr", cdddr_proc, 1);
    add_unsafe_procedure("$caaaar", caaaar_proc, 1);
    add_unsafe_procedure("$caaadr", caaadr_proc, 1);
    add_unsafe_procedure("$caadar", caadar_proc, 1);
    add_unsafe_procedure("$caaddr", caaddr_proc, 1);
    add_unsafe_procedure("$cadaar", cadaar_proc, 1);
    add_unsafe_procedure("$cadadr", cadadr_proc, 1);
    add_unsafe_procedure("$caddar", caddar_proc, 1);
    add_unsafe_procedure("$cadddr", cadddr_proc, 1);
    add_unsafe_procedure("$cdaaar", cdaaar_proc, 1);
    add_unsafe_procedure("$cdaadr", cdaadr_proc, 1);
    add_unsafe_procedure("$cdadar", cdadar_proc, 1);
    add_unsafe_procedure("$cdaddr", cdaddr_proc, 1);
    add_unsafe_procedure("$cddaar", cddaar_proc, 1);
    add_unsafe_procedure("$cddadr", cddadr_proc, 1);
    add_unsafe_procedure("$cdddar", cdddar_proc, 1);
    add_unsafe_procedure("$cddddr", cddddr_proc, 1);

    add_unsafe_procedure("$set-car!", set_car_proc, 2);
    add_unsafe_procedure("$set-cdr!", set_cdr_proc, 2);

    add_unsafe_procedure("$make-list", make_list_proc, 2);
    add_unsafe_procedure("$length", length_proc, 1);
    add_unsafe_procedure("$reverse", list_reverse, 1);
    add_unsafe_procedure("$append2", list_append2, 2);

    add_unsafe_procedure("vector?", vectorp_proc, 1);
    add_unsafe_procedure("$make-vector", make_vector, 2);
    add_unsafe_procedure("$vector-length", vector_length, 1);
    add_unsafe_procedure("$vector-ref", vector_ref, 2);
    add_unsafe_procedure("$vector-set!", vector_set, 3);
    add_unsafe_procedure("$vector-fill!", vector_fill, 2);
    add_unsafe_procedure("$list->vector", list_to_vector, 1);
    add_unsafe_procedure("$vector->list", vector_to_list, 1);

    add_unsafe_procedure("fixnum?", fixnump_proc, 1);
    add_unsafe_procedure("$fxneg", fx_neg, 1);
    add_unsafe_procedure("$fx2+", fx2_add, 2);
    add_unsafe_procedure("$fx2-", fx2_sub, 2);
    add_unsafe_procedure("$fx2*", fx2_mul, 2);
    add_unsafe_procedure("$fx2/", fx2_div, 2);
    add_unsafe_procedure("$fxremainder", fx_remainder, 2);
    add_unsafe_procedure("$fxmodulo", fx_modulo, 2);
    
    add_unsafe_procedure("$fx2=", fx2_eq, 2);
    add_unsafe_procedure("$fx2>", fx2_gt, 2);
    add_unsafe_procedure("$fx2<", fx2_lt, 2);
    add_unsafe_procedure("$fx2>=", fx2_ge, 2);
    add_unsafe_procedure("$fx2<=", fx2_le, 2);

    add_unsafe_procedure("string?", stringp_proc, 1);
    add_procedure("make-string", Mstring_proc, 1, 2);
    add_procedure("string", string_proc, 0, ARG_MAX);
    add_procedure("string-length", string_length_proc, 1, 1);
    add_procedure("string-ref", string_ref_proc, 2, 2);
    add_procedure("string-set!", string_set_proc, 3, 3);
    add_procedure("string-append", string_append_proc, 0, ARG_MAX);
    add_procedure("format", format_proc, 1, ARG_MAX);
    add_procedure("number->string", number_to_string_proc, 1, 1);
    add_procedure("string->number", string_to_number_proc, 1, 1);

    add_unsafe_procedure("record?", recordp_proc, 1);
    add_unsafe_procedure("record-type-descriptor?", record_rtdp_proc, 1);
    add_procedure("make-record-type-descriptor", make_rtd_proc, 6, 6);
    add_procedure("record-type-name", record_type_name_proc, 1, 1);
    add_procedure("record-type-parent", record_type_parent_proc, 1, 1);
    add_procedure("record-type-uid", record_type_uid_proc, 1, 1);
    add_procedure("record-type-opaque?", record_type_opaque_proc, 1, 1);
    add_procedure("record-type-sealed?", record_type_sealed_proc, 1, 1);
    add_procedure("record-type-field-names", record_type_fields_proc, 1, 1);
    add_procedure("record-type-field-mutable?", record_type_field_mutable_proc, 2, 2);
    add_unsafe_procedure("$record-value?", record_valuep_proc, 1);
    add_procedure("$make-record", Mrecord_proc, 1, ARG_MAX);
    add_procedure("$record-rtd", record_rtd_proc, 1, 1);
    add_procedure("$record-ref", record_ref_proc, 2, 2);
    add_procedure("$record-set!", record_set_proc, 3, 3);
    add_procedure("$current-record-equal-procedure", current_record_equal_procedure_proc, 0, 1);
    add_procedure("$current-record-hash-procedure", current_record_hash_procedure_proc, 0, 1);
    add_procedure("$current-record-write-procedure", current_record_write_procedure_proc, 0, 1);

    add_unsafe_procedure("box?", boxp_proc, 1);
    add_unsafe_procedure("box", box_proc, 1);
    add_unsafe_procedure("$unbox", unbox_proc, 1);
    add_unsafe_procedure("$set-box!", box_set_proc, 2);

    add_unsafe_procedure("hashtable?", hashtablep_proc, 1);
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

    add_value(env, "eq?", eq_proc_obj);
    add_value(env, "equal?", equal_proc_obj);
    add_value(env, "eq-hash", eq_hash_proc_obj);
    add_value(env, "equal-hash", equal_hash_proc_obj);
    
    add_unsafe_procedure("syntax?", syntaxp_proc, 1);
    add_procedure("syntax-e", syntax_e_proc, 1, 1);
    add_procedure("syntax-loc", syntax_loc_proc, 2, 2);
    add_procedure("datum->syntax", to_syntax_proc, 1, 1);
    add_procedure("syntax->datum", to_datum_proc, 1, 1);
    add_procedure("syntax->list", syntax_to_list_proc, 1, 1);

    add_unsafe_procedure("pattern-variable?", patternp_proc, 1);
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
    add_unsafe_procedure("void", void_proc, 0);

    add_procedure("call-with-values", call_with_values_proc, 2, 2);
    add_procedure("values", values_proc, 0, ARG_MAX);

    add_unsafe_procedure("input-port?", input_portp_proc, 1);
    add_unsafe_procedure("output-port?", output_portp_proc, 1);
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

    // Load the prelude
    load_file(PRELUDE_PATH, env);
}

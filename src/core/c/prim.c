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
    add_unsafe_procedure("$make-string", make_string, 2);
    add_unsafe_procedure("$string-length", string_length, 1);
    add_unsafe_procedure("$string-ref", string_ref, 2);
    add_unsafe_procedure("$string-set!", string_set, 3);
    add_unsafe_procedure("$number->string", number_to_string, 1);
    add_unsafe_procedure("$string->number", string_to_number, 1);
    add_unsafe_procedure("$symbol->string", symbol_to_string, 1);
    add_unsafe_procedure("$string->symbol", string_to_symbol, 1);
    add_unsafe_procedure("$list->string", list_to_string, 1);
    add_unsafe_procedure("$string->list", string_to_list, 1);

    add_unsafe_procedure("record?", recordp_proc, 1);
    add_unsafe_procedure("record-type-descriptor?", record_rtdp_proc, 1);
    add_unsafe_procedure("$make-record-type-descriptor", make_rtd, 6);
    add_unsafe_procedure("$record-type-name", rtd_name, 1);
    add_unsafe_procedure("$record-type-parent", rtd_parent, 1);
    add_unsafe_procedure("$record-type-uid", rtd_uid, 1);
    add_unsafe_procedure("$record-type-sealed?", rtd_sealedp, 1);
    add_unsafe_procedure("$record-type-opaque?", rtd_opaquep, 1);
    add_unsafe_procedure("$record-type-length", rtd_length, 1);
    add_unsafe_procedure("$record-type-field-names", rtd_fields, 1);
    add_unsafe_procedure("$record-type-field-mutable?", rtd_field_mutablep, 2);

    add_unsafe_procedure("$record-value?", record_valuep_proc, 1);
    add_unsafe_procedure("$make-record", make_record, 2);
    add_unsafe_procedure("$record-rtd", record_rtd_proc, 1);
    add_unsafe_procedure("$record-ref", record_ref_proc, 2);
    add_unsafe_procedure("$record-set!", record_set_proc, 3);

    add_unsafe_procedure("$default-record-equal-procedure", default_record_equal_proc, 0);
    add_unsafe_procedure("$default-record-equal-procedure-set!", default_record_equal_set_proc, 1);
    add_unsafe_procedure("$default-record-hash-procedure", default_record_hash_proc, 0);
    add_unsafe_procedure("$default-record-hash-procedure-set!", default_record_hash_set_proc, 1);

    add_unsafe_procedure("box?", boxp_proc, 1);
    add_unsafe_procedure("box", box_proc, 1);
    add_unsafe_procedure("$unbox", unbox_proc, 1);
    add_unsafe_procedure("$set-box!", box_set_proc, 2);

    add_unsafe_procedure("$hashtable?", hashtablep_proc, 1);
    add_unsafe_procedure("$make-hashtable", make_hashtable, 1);
    add_unsafe_procedure("$hashtable-copy", hashtable_copy, 1);
    add_unsafe_procedure("$hashtable-cells", hashtable_cells, 1);
    add_unsafe_procedure("$hashtable-size", hashtable_size, 1);
    add_unsafe_procedure("$hashtable-size-set!", hashtable_size_set, 2);
    add_unsafe_procedure("$hashtable-length", hashtable_length, 1);
    add_unsafe_procedure("$hashtable-ref", hashtable_ref, 2);
    add_unsafe_procedure("$hashtable-set!", hashtable_set, 3);
    add_unsafe_procedure("$hashtable-clear!", hashtable_clear, 1);

    add_unsafe_procedure("eq?", eq_proc, 2);
    add_unsafe_procedure("$equal?", equal_proc, 2);
    add_unsafe_procedure("eq-hash", eq_hash_proc, 1);
    add_unsafe_procedure("$equal-hash", equal_hash_proc, 1);
    
    add_unsafe_procedure("syntax?", syntaxp_proc, 1);
    add_unsafe_procedure("$syntax-e", syntax_e_proc, 1);
    add_unsafe_procedure("$syntax-loc", syntax_loc_proc, 1);
    add_unsafe_procedure("datum->syntax", to_syntax, 1);
    add_unsafe_procedure("$syntax->datum", strip_syntax, 1);
    add_unsafe_procedure("$syntax->list", syntax_to_list, 1);

    add_unsafe_procedure("environment?", environmentp_proc, 1);
    add_unsafe_procedure("interaction-environment", interaction_environment, 0);
    add_unsafe_procedure("null-environment", empty_environment, 0);
    add_unsafe_procedure("environment", environment_proc, 0);
    add_unsafe_procedure("current-environment", current_environment, 0);
    add_unsafe_procedure("$environment-extend", extend_environment, 1);
    add_unsafe_procedure("$environment-names", environment_names, 1);
    add_unsafe_procedure("$environment-ref", environment_variable_ref, 3);
    add_unsafe_procedure("$environment-set!", environment_variable_set, 3);

    add_unsafe_procedure("procedure-arity", procedure_arity_proc, 1);
    add_unsafe_procedure("identity", identity_proc, 1);
    add_unsafe_procedure("void", void_proc, 0);

    add_procedure("eval", eval_proc, 1, 2);
    add_procedure("apply", apply_proc, 2, ARG_MAX);

    add_procedure("call-with-values", call_with_values_proc, 2, 2);
    add_procedure("values", values_proc, 0, ARG_MAX);

    add_unsafe_procedure("input-port?", input_portp_proc, 1);
    add_unsafe_procedure("output-port?", output_portp_proc, 1);
    add_unsafe_procedure("string-port?", string_portp_proc, 1);
    add_unsafe_procedure("current-input-port", current_input_port, 0);
    add_unsafe_procedure("current-output-port", current_output_port, 0);
    add_unsafe_procedure("$open-input-file", open_input_file, 1);
    add_unsafe_procedure("$open-output-file", open_output_file, 1);
    add_unsafe_procedure("$open-input-string", open_input_string, 1);
    add_unsafe_procedure("open-output-string", open_output_string, 0);
    add_unsafe_procedure("$close-input-port", close_input_port, 1);
    add_unsafe_procedure("$close-output-port", close_output_port, 1);
    add_unsafe_procedure("$get-output-string", get_output_string, 1);

    add_unsafe_procedure("$read", read_proc, 1);
    add_unsafe_procedure("$read-char", read_char_proc, 1);
    add_unsafe_procedure("$peek-char", peek_char_proc, 1);
    add_unsafe_procedure("$char-ready?", char_readyp_proc, 1);
    add_unsafe_procedure("$put-char", put_char_proc, 2);
    add_unsafe_procedure("$put-string", put_string_proc, 4);
    add_unsafe_procedure("$flush-output-port", flush_output_proc, 1);
    add_unsafe_procedure("$newline", newline_proc, 1);

    add_unsafe_procedure("$fasl-read", fasl_read_proc, 1);
    add_unsafe_procedure("$fasl-write", fasl_write_proc, 2);
    
    add_unsafe_procedure("$load", load_proc, 1);
    add_unsafe_procedure("$exit", exit_proc, 1);
    add_unsafe_procedure("version", version_proc, 0);
    add_unsafe_procedure("command-line", command_line_proc, 0);
    add_unsafe_procedure("$current-directory", current_directory_proc, 0);
    add_unsafe_procedure("$current-directory-set!", current_directory_set_proc, 1);

    add_procedure("error", error_proc, 2, ARG_MAX);
    add_procedure("syntax-error", syntax_error_proc, 2, 4);

    // Load the prelude
    load_file(PRELUDE_PATH, env);
}

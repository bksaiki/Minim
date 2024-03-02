/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

#define add_value(env, name, c_val)  \
    env_define_var(env, intern(name), c_val);

mobj make_env() {
    mobj env = setup_env();
    add_value(env, "null", minim_null);
    add_value(env, "true", minim_true);
    add_value(env, "false", minim_false);
    add_value(env, "eof", minim_eof);
    init_prims(env);
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

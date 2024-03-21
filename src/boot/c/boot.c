/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

void minim_boot_init() {
    minim_thread *th;
    
    GC_pause();

    // initialize globals
    globals = GC_alloc(sizeof(minim_globals));
    globals->symbols = make_intern_table();
    globals->current_thread = GC_alloc(sizeof(minim_thread));

    // initialize special symbols

    quote_symbol = intern("quote");
    define_values_symbol = intern("define-values");
    let_values_symbol = intern("let-values");
    letrec_values_symbol = intern("letrec-values");
    setb_symbol = intern("set!");
    if_symbol = intern("if");
    lambda_symbol = intern("lambda");
    begin_symbol = intern("begin");
    quote_syntax_symbol = intern("quote-syntax");
    values_symbol = intern("values");

    apply_symbol = intern("#%apply");
    bind_symbol = intern("#%bind");
    bind_values_symbol = intern("#%bind-values");
    bind_values_top_symbol = intern("#%bind-values/top");
    brancha_symbol = intern("#%brancha");
    branchf_symbol = intern("#%branchf");
    check_arity_symbol = intern("#%check-arity");
    check_stack_symbol = intern("#%check-stack");
    clear_frame_symbol = intern("#%clear-frame");
    do_apply_symbol = intern("#%do-apply");
    do_rest_symbol = intern("#%do-rest");
    do_values_symbol = intern("#%do-values");
    get_arg_symbol = intern("#%get-arg");
    literal_symbol = intern("#%literal");
    lookup_symbol = intern("#%lookup");
    make_closure_symbol = intern("#%make-closure");
    make_env_symbol = intern("#%make-env");
    pop_symbol = intern("#%pop");
    pop_env_symbol = intern("#%pop-env");
    push_symbol = intern("#%push");
    push_env_symbol = intern("#%push-env");
    rebind_symbol = intern("#%rebind");
    ret_symbol = intern("#%ret");
    save_cc_symbol = intern("#%save-cc");
    set_proc_symbol = intern("#%set-proc");

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

    // initialize thread

    th = current_thread();
    th->pid = 0;    // TODO

    global_env(th) = make_env();
    current_continuation(th) = minim_null;
    current_segment(th) = Mstack_segment(NULL, stack_seg_default_size);
    current_sfp(th) = stack_seg_base(current_segment(th));
    current_cp(th) = NULL;
    current_ac(th) = 0;

    input_port(th) = Minput_port(stdin);
    output_port(th) = Moutput_port(stdout);
    current_directory(th) = Mstring(get_current_dir());
    command_line(th) = minim_null;
    record_equal_proc(th) = minim_false;
    record_hash_proc(th) = minim_false;

    values_buffer(th) = GC_alloc(INIT_VALUES_BUFFER_LEN * sizeof(mobj));
    values_buffer_size(th) = INIT_VALUES_BUFFER_LEN;
    values_buffer_count(th) = 0;

    // GC roots

    GC_register_root(minim_null);
    GC_register_root(minim_empty_vec);
    GC_register_root(minim_true);
    GC_register_root(minim_false);
    GC_register_root(minim_eof);
    GC_register_root(minim_void);
    GC_register_root(minim_values);
    GC_register_root(globals);

    GC_resume();
}

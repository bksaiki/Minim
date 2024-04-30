// global.c: global variables

#include "../minim.h"

intern_table *symbols;
mobj *curr_thread_ref;

void init_minim() {
    // interned symbol table
    symbols = make_intern_table();
    GC_register_root(symbols);
    // pointer to a thread context
    curr_thread_ref = GC_alloc(sizeof(mobj));
    GC_register_root(curr_thread_ref);
    current_tc() = NULL; // to be initialized later

    // initialize special symbols
    begin_symbol = intern("begin");
    call_with_values_symbol = intern("call-with-values");
    case_lambda_symbol = intern("case-lambda");
    define_values_symbol = intern("define-values");
    if_symbol = intern("if");
    lambda_symbol = intern("lambda");
    let_values_symbol = intern("let-values");
    letrec_values_symbol = intern("letrec-values");
    quote_symbol = intern("quote");
    quote_syntax_symbol = intern("quote-syntax");
    setb_symbol = intern("set!");
    values_symbol = intern("values");

    mvcall_symbol = intern("#%mv-call");
    mvlet_symbol = intern("#%mv-let");
    mvvalues_symbol = intern("#%mv-values");

    apply_symbol = intern("#%apply");
    bind_symbol = intern("#%bind");
    bind_values_symbol = intern("#%bind-values");
    brancha_symbol = intern("#%brancha");
    branchf_symbol = intern("#%branchf");
    branchgt_symbol = intern("#%branchgt");
    branchlt_symbol = intern("#%branchlt");
    branchne_symbol = intern("#%branchne");
    ccall_symbol = intern("#%ccall");
    check_stack_symbol = intern("#%check-stack");
    clear_frame_symbol = intern("#%clear-frame");
    do_apply_symbol = intern("#%do-apply");
    do_arity_error_symbol = intern("#%do-arity-error");
    do_eval_symbol = intern("#%do-eval");
    do_raise_symbol = intern("#%do-raise");
    do_rest_symbol = intern("#%do-rest");
    do_values_symbol = intern("#%do-values");
    do_with_values_symbol = intern("#%do-with-values");
    get_ac_symbol = intern("#%get-ac");
    get_arg_symbol = intern("#%get-arg");
    get_tenv_symbol = intern("#%get-tenv");
    literal_symbol = intern("#%literal");
    lookup_symbol = intern("#%lookup");
    tl_bind_values_symbol = intern("#%tl-bind-values");
    tl_lookup_symbol = intern("#%tl-lookup");
    make_closure_symbol = intern("#%make-closure");
    make_env_symbol = intern("#%make-env");
    make_unbound_symbol = intern("#%make-unbound");
    mov_symbol = intern("#%mov");
    pop_symbol = intern("#%pop");
    pop_env_symbol = intern("#%pop-env");
    push_symbol = intern("#%push");
    push_env_symbol = intern("#%push-env");
    rebind_symbol = intern("#%rebind");
    ret_symbol = intern("#%ret");
    save_cc_symbol = intern("#%save-cc");
    set_arg_symbol = intern("#%set-arg");
    set_tenv_symbol = intern("#%set-tenv");
    set_proc_symbol = intern("#%set-proc");

    // initialize special values    

    minim_null = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_null);
    minim_type(minim_null) = MINIM_OBJ_SPECIAL;
    minim_true = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_true);
    minim_type(minim_true) = MINIM_OBJ_SPECIAL;
    minim_false = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_false);
    minim_type(minim_false) = MINIM_OBJ_SPECIAL;
    minim_eof = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_eof);
    minim_type(minim_eof) = MINIM_OBJ_SPECIAL;
    minim_void = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_void);
    minim_type(minim_void) = MINIM_OBJ_SPECIAL;
    minim_values = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_values);
    minim_type(minim_values) = MINIM_OBJ_SPECIAL;
    minim_unbound = GC_alloc(sizeof(mbyte));
    GC_register_root(minim_unbound);
    minim_type(minim_unbound) = MINIM_OBJ_SPECIAL;
    minim_empty_vec = Mvector(0, NULL);
    GC_register_root(minim_empty_vec);

    minim_base_rtd = Mrecord(NULL, record_rtd_min_size);
    GC_register_root(minim_base_rtd);

    record_rtd_name(minim_base_rtd) = Mstring("$base-record-type");
    record_rtd_parent(minim_base_rtd) = minim_base_rtd;
    record_rtd_uid(minim_base_rtd) = minim_false;
    record_rtd_opaque(minim_base_rtd) = minim_true;
    record_rtd_sealed(minim_base_rtd) = minim_true;
    record_rtd_protocol(minim_base_rtd) = minim_false;

    init_envs();
}


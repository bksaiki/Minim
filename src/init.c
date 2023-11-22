// init.c: initialization of Minim's runtime

#include "minim.h"

struct _M_globals M_glob;

void minim_init() {
    // initialize special values
    minim_null = GC_alloc_atomic(1);
    GC_register_root(minim_null, 1);
    minim_true = GC_alloc_atomic(1);
    GC_register_root(minim_true, 1);
    minim_false = GC_alloc_atomic(1);
    GC_register_root(minim_false, 1);
    minim_eof = GC_alloc_atomic(1);
    GC_register_root(minim_eof, 1);
    minim_void = GC_alloc_atomic(1);
    GC_register_root(minim_void, 1);

    minim_type(minim_null) = MINIM_OBJ_SPECIAL;
    minim_type(minim_true) = MINIM_OBJ_SPECIAL;
    minim_type(minim_false) = MINIM_OBJ_SPECIAL;
    minim_type(minim_eof) = MINIM_OBJ_SPECIAL;
    minim_type(minim_void) = MINIM_OBJ_SPECIAL;
    
    // initialize globals
    GC_register_root(&M_glob, sizeof(M_glob));
    M_glob.null_string = Mstring("");
    M_glob.null_vector = Mvector(0, NULL);
    M_glob.thread = GC_alloc(sizeof(mthread));
    M_glob.primlist = Mvector(PRIM_TABLE_SIZE, minim_null);
    intern_table_init();

    // initialize thread
    th_input_port(M_glob.thread) = Mport(stdin, PORT_FLAG_READ | PORT_FLAG_OPEN);
    th_output_port(M_glob.thread) = Mport(stdout, PORT_FLAG_OPEN);
    th_error_port(M_glob.thread) = Mport(stderr, PORT_FLAG_OPEN);
    th_working_dir(M_glob.thread) = Mstring(get_current_dir());

    // get special symbols
    define_values_sym = intern("define-values");
    letrec_values_sym = intern("letrec-values");
    let_values_sym = intern("let-values");
    values_sym = intern("values");
    lambda_sym = intern("lambda");
    begin_sym = intern("begin");
    if_sym = intern("if");
    quote_sym = intern("quote");
    setb_sym = intern("set!");
    void_sym = intern("void");

    define_sym = intern("define");
    letrec_sym = intern("letrec");
    let_sym = intern("let");
    cond_sym = intern("cond");
    and_sym = intern("and");
    or_sym = intern("or");
    foreign_proc_sym = intern("#%foreign-procedure");

    // primitive table
    prim_table_init();
    runtime_init();
}

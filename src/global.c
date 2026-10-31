// global.c: global objects

#include "minim.h"

obj Mnull;
obj Mtrue;
obj Mfalse;
obj Mvoid;
obj Meof;
obj Memptyvec;

obj Mvalues;
obj Munbound;

obj Mand_symbol;
obj Mbegin_symbol;
obj Mcase_lambda_symbol;
obj Mcond_symbol;
obj Mif_symbol;
obj Mlambda_symbol;
obj Mlet_symbol;
obj Mlet_values_symbol;
obj Mletrec_symbol;
obj Mletrec_values_symbol;
obj Mor_symbol;
obj Mquote_symbol;
obj Msetb_symbol;
obj Mwhen_symbol;
obj Munless_symbol;

obj Mdefine_symbol;
obj Mdefine_values_symbol;
obj Mexport_symbol;
obj Mimport_symbol;
obj Mkernel_symbol;

intern_table *itab;
obj *Mcurr_tc_box;
obj Mkernel;

void minim_init(void) {
    // special values
    Mnull = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Mnull) = SPECIAL_OBJ_TYPE;
    Mtrue = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Mtrue) = SPECIAL_OBJ_TYPE;
    Mfalse = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Mfalse) = SPECIAL_OBJ_TYPE;
    Mvoid = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Mvoid) = SPECIAL_OBJ_TYPE;
    Meof = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Meof) = SPECIAL_OBJ_TYPE;

    Memptyvec = GC_malloc_uncollectable(Mvector_size(0));
    obj_type(Memptyvec) = VECTOR_OBJ_TYPE;
    Mvector_len(Memptyvec) = 0;

    Mvalues = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Mvalues) = SPECIAL_OBJ_TYPE;
    Munbound = GC_malloc_uncollectable(sizeof(byte));
    obj_type(Munbound) = SPECIAL_OBJ_TYPE;

    // intern table

    itab = make_intern_table();
    GC_add_roots(itab, ptr_add(itab, sizeof(intern_table)));

    // intern symbols

    Mand_symbol = Mintern("and");
    Mbegin_symbol = Mintern("begin");
    Mcond_symbol = Mintern("cond");
    Mcase_lambda_symbol = Mintern("case-lambda");
    Mif_symbol = Mintern("if");
    Mlambda_symbol = Mintern("lambda");
    Mlet_symbol = Mintern("let");
    Mlet_values_symbol = Mintern("let-values");
    Mletrec_symbol = Mintern("letrec");
    Mletrec_values_symbol = Mintern("letrec-values");
    Mor_symbol = Mintern("or");
    Mquote_symbol = Mintern("quote");
    Msetb_symbol = Mintern("set!");
    Mwhen_symbol = Mintern("when");
    Munless_symbol = Mintern("unless");

    Mdefine_symbol = Mintern("define");
    Mdefine_values_symbol = Mintern("define-values");
    Mexport_symbol = Mintern("export");
    Mimport_symbol = Mintern("import");
    Mkernel_symbol = Mintern("#%kernel");

    // initialize thread context
    Mcurr_tc_box = GC_malloc_uncollectable(sizeof(obj));
    Mcurr_tc() = Mthread_context();

    // initialize primitives
    init_prims();
}


NORETURN void minim_shutdown(int code) {
    GC_deinit();
    exit(code);
}

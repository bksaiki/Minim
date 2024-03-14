/*
    Procedures
*/

#include "../minim.h"

//
//  Primitives
//

mobj Mclosure(mobj args, mobj body, mobj env, short min_arity, short max_arity) {
    mobj o = GC_alloc(minim_closure_size);
    minim_type(o) = MINIM_OBJ_CLOSURE;
    minim_closure_args(o) = args;
    minim_closure_body(o) = body;
    minim_closure_env(o) = env;
    minim_closure_argc_low(o) = min_arity;
    minim_closure_argc_high(o) = max_arity;
    minim_closure_name(o) = NULL;
    return o;
}

//
//  Procedure
//

mobj procp_proc(mobj x) {
    return minim_procp(x) ? minim_true : minim_false;
}

mobj call_with_values_proc() {
    uncallable_prim_exn("call-with-values");
}

mobj values_proc() {
    uncallable_prim_exn("values");
}

mobj eval_proc() {
    uncallable_prim_exn("eval");
}

mobj apply_proc() {
    uncallable_prim_exn("apply");
}

mobj identity_proc(mobj x) {
    return x;
}

mobj procedure_arity_proc(mobj proc) {
    // (-> procedure any)
    int min_arity, max_arity;

    if (minim_primp(proc)) {
        min_arity = minim_prim_argc_low(proc);
        max_arity = minim_prim_argc_high(proc);
    } else /* minim_closurep(proc) */ {
        min_arity = minim_closure_argc_low(proc);
        max_arity = minim_closure_argc_high(proc);
    }

    if (min_arity == max_arity) {
        return Mfixnum(min_arity);
    } else if (max_arity == ARG_MAX) {
        return Mcons(Mfixnum(min_arity), minim_false);
    } else {
        return Mcons(Mfixnum(min_arity), Mfixnum(max_arity));
    }
}

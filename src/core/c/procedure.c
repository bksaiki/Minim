/*
    Procedures
*/

#include "../minim.h"

//
//  Primitives
//

mobj Mclosure(mobj env, mobj code) {
    mobj o = GC_alloc(minim_closure_size);
    minim_type(o) = MINIM_OBJ_CLOSURE;
    minim_closure_env(o) = env;
    minim_closure_code(o) = code;
    return o;
}

//
//  Procedure
//

mobj procp_proc(mobj x) {
    return minim_procp(x) ? minim_true : minim_false;
}

mobj procedure_arity_proc(mobj proc) {
    // (-> procedure any)
    if (minim_primp(proc)) {
        return minim_prim_arity(proc);
    } else {
        return minim_code_arity(minim_closure_code(proc));
    }
}

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
    minim_closure_name(o) = minim_false;
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
    return minim_code_arity(minim_closure_code(proc));
}

mobj procedure_name_proc(mobj proc) {
    // (-> procedure (or #f symbol?))
    return minim_closure_name(proc);
}

mobj procedure_rename_proc(mobj proc, mobj id) {
    // (-> procedure symbol? procedure
    proc = Mclosure(minim_closure_env(proc), minim_closure_code(proc));
    minim_closure_name(proc) = id;
    return proc;
}

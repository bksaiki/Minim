/*
    Procedures
*/

#include "../minim.h"

//
//  Primitives
//

mobj Mclosure(mobj env, mobj code, size_t free_count) {
    mobj o = GC_alloc(minim_closure_size(free_count));
    minim_type(o) = MINIM_OBJ_CLOSURE;
    minim_closure_env(o) = env;
    minim_closure_code(o) = code;
    minim_closure_name(o) = minim_false;
    minim_closure_count(o) = free_count;
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
    // (-> procedure symbol? procedure)
    mobj env, code, proc2;
    size_t num_free;

    // construct new closure
    env = minim_closure_env(proc);
    code = minim_closure_code(proc);
    num_free = minim_closure_count(proc);
    proc2 = Mclosure(env, code, num_free);

    // copy free variable cells
    for (size_t i = 0; i < num_free; i++) {
        minim_closure_ref(proc2, i) = minim_closure_ref(proc, i);
    }

    minim_closure_name(proc) = id;
    return proc;
}

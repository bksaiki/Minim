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

mobj Mnative_closure(mobj env, void *fn, short min_arity, short max_arity) {
    mobj o = GC_alloc(minim_native_closure_size);
    minim_type(o) = MINIM_OBJ_NATIVE_CLOSURE;
    minim_native_closure(o) = fn;
    minim_native_closure_env(o) = env;
    minim_native_closure_argc_low(o) = min_arity;
    minim_native_closure_argc_high(o) = max_arity;
    minim_native_closure_name(o) = NULL;
    return o;
}

// Resizes the value buffer if need be
void resize_values_buffer(minim_thread *th, int size) {
    values_buffer(th) = GC_alloc(size * sizeof(mobj*));
    values_buffer_size(th) = size;
}

//
//  Procedure
//

mobj procp_proc(mobj x) {
    return minim_procp(x) ? minim_true : minim_false;
}

mobj call_with_values_proc(int argc, mobj *args) {
    uncallable_prim_exn("call-with-values");
}

mobj values_proc(int argc, mobj *args) {
    // (-> any ... (values any ...))
    minim_thread *th;

    th = current_thread();
    if (argc > values_buffer_size(th))
        resize_values_buffer(th, argc);
    
    values_buffer_count(th) = argc;
    memcpy(values_buffer(th), args, argc * sizeof(mobj *));    
    return minim_values;
}

mobj eval_proc(int argc, mobj *args) {
    uncallable_prim_exn("eval");
}

mobj apply_proc(int argc, mobj *args) {
    uncallable_prim_exn("apply");
}

mobj identity_proc(mobj x) {
    return x;
}

mobj procedure_arity_proc(int argc, mobj *args) {
    // (-> procedure any)
    mobj proc;
    int min_arity, max_arity;

    proc = args[0];
    if (!minim_primp(proc) && !minim_closurep(proc))
        bad_type_exn("procedure-arity", "procedure?", proc);

    if (minim_primp(proc)) {
        min_arity = minim_prim_argc_low(proc);
        max_arity = minim_prim_argc_high(proc);
    } else if (minim_prim2p(proc)) {
        min_arity = minim_prim2_argc(proc);
        max_arity = minim_prim2_argc(proc);
    } else if (minim_closurep(proc)) {
        min_arity = minim_closure_argc_low(proc);
        max_arity = minim_closure_argc_high(proc);
    } else {
        min_arity = minim_native_closure_argc_low(proc);
        max_arity = minim_native_closure_argc_high(proc);
    }

    if (min_arity == max_arity) {
        return Mfixnum(min_arity);
    } else if (max_arity == ARG_MAX) {
        return Mcons(Mfixnum(min_arity), minim_false);
    } else {
        return Mcons(Mfixnum(min_arity), Mfixnum(max_arity));
    }
}

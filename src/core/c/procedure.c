/*
    Procedures
*/

#include "../minim.h"

//
//  Primitives
//

static void set_arity(proc_arity *arity, short min_arity, short max_arity) {
    if (min_arity > ARG_MAX || max_arity > ARG_MAX) {
        fprintf(stderr, "primitive procedure intialized with too large an arity: [%d, %d]", min_arity, max_arity);
        minim_shutdown(1);
    } else {
        arity->arity_min = min_arity;
        arity->arity_max = max_arity;
        if (min_arity == max_arity)
            arity->type = PROC_ARITY_FIXED;
        else if (max_arity == ARG_MAX)
            arity->type = PROC_ARITY_UNBOUNDED;
        else
            arity->type = PROC_ARITY_RANGE;
    }
}

mobj *make_prim_proc(minim_prim_t proc,
                             char *name,
                             short min_arity, short max_arity) {
    minim_prim_object *o = GC_alloc(sizeof(minim_prim_object));
    o->type = minim_prim_TYPE;
    o->fn = proc;
    o->name = name;
    set_arity(&o->arity, min_arity, max_arity);

    return ((mobj *) o);
}

mobj *Mclosure(mobj *args,
                           mobj *body,
                           mobj *env,
                           short min_arity,
                           short max_arity) {
    minim_closure_object *o = GC_alloc(sizeof(minim_closure_object));
    o->type = MINIM_CLOSURE_PROC_TYPE;
    o->args = args;
    o->body = body;
    o->env = env;
    o->name = NULL;
    set_arity(&o->arity, min_arity, max_arity);
    return ((mobj *) o);
}

mobj *make_native_closure(mobj *env, void *fn, short min_arity, short max_arity) {
    minim_native_closure_object *o = GC_alloc(sizeof(minim_native_closure_object));
    o->type = MINIM_NATIVE_CLOSURE_TYPE;
    o->env = env;
    o->fn = fn;
    o->name = NULL;
    set_arity(&o->arity, min_arity, max_arity);
    return ((mobj *) o);
}

// Resizes the value buffer if need be
void resize_values_buffer(minim_thread *th, int size) {
    values_buffer(th) = GC_alloc(size * sizeof(mobj*));
    values_buffer_size(th) = size;
}

//
//  Procedure
//

mobj *is_procedure_proc(int argc, mobj *args) {
    // (-> any boolean)
    mobj *o = args[0];
    return (minim_is_prim_proc(o) || minim_is_closure(o)) ? minim_true : minim_false;
}

mobj *call_with_values_proc(int argc, mobj *args) {
    uncallable_prim_exn("call-with-values");
}

mobj *values_proc(int argc, mobj *args) {
    // (-> any ... (values any ...))
    minim_thread *th;

    th = current_thread();
    if (argc > values_buffer_size(th))
        resize_values_buffer(th, argc);
    
    values_buffer_count(th) = argc;
    memcpy(values_buffer(th), args, argc * sizeof(mobj *));    
    return minim_values;
}

mobj *eval_proc(int argc, mobj *args) {
    uncallable_prim_exn("eval");
}

mobj *apply_proc(int argc, mobj *args) {
    uncallable_prim_exn("apply");
}

mobj *identity_proc(int argc, mobj *args) {
    // (-> any any)
    return args[0];
}

mobj *procedure_arity_proc(int argc, mobj *args) {
    // (-> procedure any)
    mobj *proc;
    proc_arity* arity;
    int min_arity, max_arity;

    proc = args[0];
    if (!minim_is_prim_proc(proc) && !minim_is_closure(proc))
        bad_type_exn("procedure-arity", "procedure?", proc);

    if (minim_is_prim_proc(proc)) {
        arity = &minim_prim_arity(proc);
    } else if (minim_is_closure(proc)) {
        arity = &minim_closure_arity(proc);
    } else {
        fprintf(stderr, "procedure-arity_proc(): unreachable");
        minim_shutdown(1);
    }

    min_arity = proc_arity_min(arity);
    max_arity = proc_arity_max(arity);
    if (proc_arity_is_fixed(arity)) {
        return Mfixnum(min_arity);
    } else if (proc_arity_is_unbounded(arity)) {
        return Mcons(Mfixnum(min_arity), minim_false);
    } else {
        return Mcons(Mfixnum(min_arity), Mfixnum(max_arity));   
    }
}

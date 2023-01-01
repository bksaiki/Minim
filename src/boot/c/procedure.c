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

minim_object *make_prim_proc(minim_prim_proc_t proc,
                             char *name,
                             short min_arity, short max_arity) {
    minim_prim_proc_object *o = GC_alloc(sizeof(minim_prim_proc_object));
    o->type = MINIM_PRIM_PROC_TYPE;
    o->fn = proc;
    o->name = name;
    set_arity(&o->arity, min_arity, max_arity);

    return ((minim_object *) o);
}

minim_object *make_closure_proc(minim_object *args,
                                minim_object *body,
                                minim_object *env,
                                short min_arity, short max_arity) {
    minim_closure_proc_object *o = GC_alloc(sizeof(minim_closure_proc_object));
    o->type = MINIM_CLOSURE_PROC_TYPE;
    o->args = args;
    o->body = body;
    o->env = env;
    o->name = NULL;
    set_arity(&o->arity, min_arity, max_arity);
    return ((minim_object *) o);
}

// Resizes the value buffer if need be
void resize_values_buffer(minim_thread *th, int size) {
    values_buffer(th) = GC_alloc(size * sizeof(minim_object*));
    values_buffer_size(th) = size;
}

//
//  Procedure
//

minim_object *is_procedure_proc(minim_object *args) {
    // (-> any boolean)
    minim_object *o = minim_car(args);
    return (minim_is_prim_proc(o) || minim_is_closure_proc(o)) ? minim_true : minim_false;
}

minim_object *call_with_values_proc(minim_object *args) {
    fprintf(stderr, "andmap: should not be called directly");
    minim_shutdown(1);
}

minim_object *values_proc(minim_object *args) {
    minim_thread *th;
    long i, len;

    th = current_thread();
    len = list_length(args);
    if (len > values_buffer_size(th))
        resize_values_buffer(th, len);
    
    values_buffer_count(th) = len;
    for (i = 0; i < len; ++i) {
        values_buffer_ref(th, i) = minim_car(args);
        args = minim_cdr(args);
    }
    
    return minim_values;
}

minim_object *eval_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    minim_shutdown(1);
}

minim_object *apply_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    minim_shutdown(1);
}

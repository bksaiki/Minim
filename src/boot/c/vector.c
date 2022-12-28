/*
    Vectors
*/

#include "../minim.h"

minim_object *make_vector(long len, minim_object *init) {
    minim_vector_object *o;
    long i;
    
    o = GC_alloc(sizeof(minim_vector_object));
    o->type = MINIM_VECTOR_TYPE;
    o->len = len;
    o->arr = GC_alloc(len * sizeof(minim_object *));

    if (init != NULL) {
        for (i = 0; i < len; ++i)
            o->arr[i] = init;
    }

    return ((minim_object *) o);
}

//
//  Primitives
//

minim_object *is_vector_proc(minim_object *args) {
    // (-> any boolean)
    return (minim_is_vector(minim_car(args)) ? minim_true : minim_false);
}

minim_object *make_vector_proc(minim_object *args) {
    // (-> non-negative-integer vector)
    // (-> non-negative-integer any vector)
    minim_object *init;
    long len;

    if (!minim_is_fixnum(minim_car(args)) || minim_fixnum(minim_car(args)) < 0)
        bad_type_exn("make-vector", "non-negative-integer?", minim_car(args));
    len = minim_fixnum(minim_car(args));

    if (len == 0) {
        // special case:
        return minim_empty_vec;
    }

    if (minim_is_null(minim_cdr(args))) {
        // 1st case
        init = make_fixnum(0);
    } else {
        // 2nd case
        init = minim_cadr(args);
    }

    return make_vector(len, init);
}

minim_object *vector_proc(minim_object *args) {
    // (-> any ... vector)
    minim_object *v, *it;
    long len, i;
    
    len = list_length(args);
    if (len == 0) {
        return minim_empty_vec;
    } else {
        v = make_vector(len, NULL);
        for (i = 0, it = args; i < len; ++i, it = minim_cdr(it))
            minim_vector_ref(v, i) = minim_car(it);
        return v;
    }
}

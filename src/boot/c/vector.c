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

static void vector_out_of_bounds_exn(const char *name, minim_object *v, long idx) {
    fprintf(stderr, "%s, index out of bounds\n", name);
    fprintf(stderr, " length: %ld\n", minim_vector_len(v));
    fprintf(stderr, " index:  %ld\n", idx);
    minim_shutdown(1);
}

//
//  Primitives
//

minim_object *is_vector_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return (minim_is_vector(args[0]) ? minim_true : minim_false);
}

minim_object *make_vector_proc(int argc, minim_object **args) {
    // (-> non-negative-integer vector)
    // (-> non-negative-integer any vector)
    minim_object *init;
    long len;

    if (!minim_is_fixnum(args[0]) || minim_fixnum(args[0]) < 0)
        bad_type_exn("make-vector", "non-negative-integer?", args[0]);
    len = minim_fixnum(args[0]);

    if (len == 0) {
        // special case:
        return minim_empty_vec;
    }

    if (argc == 1) {
        // 1st case
        init = make_fixnum(0);
    } else {
        // 2nd case
        init = args[1];
    }

    return make_vector(len, init);
}

minim_object *vector_proc(int argc, minim_object **args) {
    // (-> any ... vector)
    minim_object *v;
    
    v = make_vector(argc, NULL);
    memcpy(minim_vector_arr(v), args, argc * sizeof(minim_object*));
    return v;
}

minim_object *vector_length_proc(int argc, minim_object **args) {
    // (-> vector non-negative-integer?)
    minim_object *v;
    
    v = args[0];
    if (!minim_is_vector(v))
        bad_type_exn("vector-length", "vector?", v);
    return make_fixnum(minim_vector_len(v));
}

minim_object *vector_ref_proc(int argc, minim_object **args) {
    // (-> vector non-negative-integer? any)
    minim_object *v, *idx;
    
    v = args[0];
    if (!minim_is_vector(v))
        bad_type_exn("vector-ref", "vector?", v);

    idx = args[1];
    if (!minim_is_fixnum(idx) || minim_fixnum(idx) < 0)
        bad_type_exn("vector-ref", "non-negative-integer?", idx);
    if (minim_fixnum(idx) >= minim_vector_len(v))
        vector_out_of_bounds_exn("vector-ref", v, minim_fixnum(idx));

    return minim_vector_ref(v, minim_fixnum(idx));
}

minim_object *vector_set_proc(int argc, minim_object **args) {
    // (-> vector non-negative-integer? any void)
    minim_object *v, *idx;
    
    v = args[0];
    if (!minim_is_vector(v))
        bad_type_exn("vector-set!", "vector?", v);

    idx = args[1];
    if (!minim_is_fixnum(idx) || minim_fixnum(idx) < 0)
        bad_type_exn("vector-set!", "non-negative-integer?", idx);
    if (minim_fixnum(idx) >= minim_vector_len(v))
        vector_out_of_bounds_exn("vector-set!", v, minim_fixnum(idx));

    minim_vector_ref(v, minim_fixnum(idx)) = args[2];
    return minim_void;
}

minim_object *vector_fill_proc(int argc, minim_object **args) {
    // (-> vector any void)
    minim_object *v, *o;
    long i;

    v = args[0];
    if (!minim_is_vector(v))
        bad_type_exn("vector-fill!", "vector?", v);

    o = args[1];
    for (i = 0; i < minim_vector_len(v); ++i)
        minim_vector_ref(v, i) = o;

    return minim_void;
}

minim_object *vector_to_list_proc(int argc, minim_object **args) {
    // (-> vector list)
    minim_object *v, *lst;
    long i;
    
    v = args[0];
    if (!minim_is_vector(v))
        bad_type_exn("vector->list", "vector?", v);

    lst = minim_null;
    for (i = minim_vector_len(v) - 1; i >= 0; --i)
        lst = make_pair(minim_vector_ref(v, i), lst);

    return lst;
}

minim_object *list_to_vector_proc(int argc, minim_object **args) {
    // (-> list vector)
    minim_object *v, *lst, *it;
    long i;
    
    lst = args[0];
    if (!is_list(lst))
        bad_type_exn("list->vector", "list?", lst);

    v = make_vector(list_length(lst), NULL);
    it = lst;

    for (i = 0; i < minim_vector_len(v); ++i) {
        minim_vector_ref(v, i) = minim_car(it);
        it = minim_cdr(it);
    }

    return v;
}

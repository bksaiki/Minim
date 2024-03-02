/*
    Vectors
*/

#include "../minim.h"

mobj Mvector(long len, mobj init) {
    mobj o = GC_alloc(minim_vector_size(len));
    minim_type(o) = MINIM_OBJ_VECTOR;
    minim_vector_len(o) = len;
    if (init != NULL) {
        for (long i = 0; i < len; ++i)
            minim_vector_ref(o, i) = init;
    }

    return o;
}

mobj list_to_vector(mobj lst) {
    mobj v, it;
    msize i;
    
    it = v = Mvector(list_length(lst), NULL);
    for (i = 0; i < minim_vector_len(v); ++i) {
        minim_vector_ref(v, i) = minim_car(it);
        it = minim_cdr(it);
    }

    return v;
}

static void vector_out_of_bounds_exn(const char *name, mobj v, long idx) {
    fprintf(stderr, "%s, index out of bounds\n", name);
    fprintf(stderr, " length: %ld\n", minim_vector_len(v));
    fprintf(stderr, " index:  %ld\n", idx);
    minim_shutdown(1);
}

//
//  Primitives
//

mobj is_vector_proc(int argc, mobj *args) {
    // (-> any boolean)
    return (minim_vectorp(args[0]) ? minim_true : minim_false);
}

mobj Mvector_proc(int argc, mobj *args) {
    // (-> non-negative-integer vector)
    // (-> non-negative-integer any vector)
    mobj init;
    long len;

    if (!minim_fixnump(args[0]) || minim_fixnum(args[0]) < 0)
        bad_type_exn("make-vector", "non-negative-integer?", args[0]);
    len = minim_fixnum(args[0]);

    if (len == 0) {
        // special case:
        return minim_empty_vec;
    }

    if (argc == 1) {
        // 1st case
        init = Mfixnum(0);
    } else {
        // 2nd case
        init = args[1];
    }

    return Mvector(len, init);
}

mobj vector_proc(int argc, mobj *args) {
    // (-> any ... vector)
    mobj v = Mvector(argc, NULL);
    for (long i = 0; i < argc; i++)
        minim_vector_ref(v, i) = args[i];
    return v;
}

mobj vector_length_proc(int argc, mobj *args) {
    // (-> vector non-negative-integer?)
    mobj v = args[0];
    if (!minim_vectorp(v))
        bad_type_exn("vector-length", "vector?", v);
    return Mfixnum(minim_vector_len(v));
}

mobj vector_ref_proc(int argc, mobj *args) {
    // (-> vector non-negative-integer? any)
    mobj v, idx;
    
    v = args[0];
    if (!minim_vectorp(v))
        bad_type_exn("vector-ref", "vector?", v);

    idx = args[1];
    if (!minim_fixnump(idx) || minim_fixnum(idx) < 0)
        bad_type_exn("vector-ref", "non-negative-integer?", idx);
    if (minim_fixnum(idx) >= minim_vector_len(v))
        vector_out_of_bounds_exn("vector-ref", v, minim_fixnum(idx));

    return minim_vector_ref(v, minim_fixnum(idx));
}

mobj vector_set_proc(int argc, mobj *args) {
    // (-> vector non-negative-integer? any void)
    mobj v, idx;
    
    v = args[0];
    if (!minim_vectorp(v))
        bad_type_exn("vector-set!", "vector?", v);

    idx = args[1];
    if (!minim_fixnump(idx) || minim_fixnum(idx) < 0)
        bad_type_exn("vector-set!", "non-negative-integer?", idx);
    if (minim_fixnum(idx) >= minim_vector_len(v))
        vector_out_of_bounds_exn("vector-set!", v, minim_fixnum(idx));

    minim_vector_ref(v, minim_fixnum(idx)) = args[2];
    return minim_void;
}

mobj vector_fill_proc(int argc, mobj *args) {
    // (-> vector any void)
    mobj v, o;
    long i;

    v = args[0];
    if (!minim_vectorp(v))
        bad_type_exn("vector-fill!", "vector?", v);

    o = args[1];
    for (i = 0; i < minim_vector_len(v); ++i)
        minim_vector_ref(v, i) = o;

    return minim_void;
}

mobj vector_to_list_proc(int argc, mobj *args) {
    // (-> vector list)
    mobj v, lst;
    long i;
    
    v = args[0];
    if (!minim_vectorp(v))
        bad_type_exn("vector->list", "vector?", v);

    lst = minim_null;
    for (i = minim_vector_len(v) - 1; i >= 0; --i)
        lst = Mcons(minim_vector_ref(v, i), lst);

    return lst;
}

mobj list_to_vector_proc(int argc, mobj *args) {
    // (-> list vector)
    mobj lst = args[0];
    if (!minim_listp(lst))
        bad_type_exn("list->vector", "list?", lst);
    return list_to_vector(lst);
}

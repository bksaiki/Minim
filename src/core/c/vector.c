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

//
//  Primitives
//

mobj vectorp_proc(mobj x) {
    // (-> any vector)
    return minim_vectorp(x) ? minim_true : minim_false;
}

mobj make_vector(mobj len, mobj init) {
    // (-> non-negative-integer any vector)
    return Mvector(minim_fixnum(len), init);
}

mobj vector_length(mobj v) {
    // (-> vector fixnum)
    return Mfixnum(minim_vector_len(v));
}

mobj vector_ref(mobj v, mobj idx) {
    // (-> vector fixnum any)
    return minim_vector_ref(v, minim_fixnum(idx));
}

mobj vector_set(mobj v, mobj idx, mobj x) {
    // (-> vector fixnum any void)
    minim_vector_ref(v, minim_fixnum(idx)) = x;
    return minim_void;
}

mobj vector_fill(mobj v, mobj x) {
    // (-> vector any void)
    for (long i = 0; i < minim_vector_len(v); i++)
        minim_vector_ref(v, i) = x;
    return minim_void;
}

mobj list_to_vector(mobj lst) {
    // (-> list vector)
    mobj v, it;
    msize i;
    
    it = lst;
    v = Mvector(list_length(lst), NULL);
    for (i = 0; i < minim_vector_len(v); ++i) {
        minim_vector_ref(v, i) = minim_car(it);
        it = minim_cdr(it);
    }

    return v;
}

mobj vector_to_list(mobj v) {
    // (-> vector list
    mobj lst = minim_null;
    for (long i = minim_vector_len(v) - 1; i >= 0; i--)
        lst = Mcons(minim_vector_ref(v, i), lst);
    return lst;
}

// vector.c: vectors

#include "minim.h"

mobj list_to_vector(mobj o) {
    size_t l = list_length(o);
    mobj v = Mvector(l, NULL);
    for (size_t i = 0; i < l; ++i) {
        minim_vector_ref(v, i) = minim_car(o);
        o = minim_cdr(o);
    }
    return v;
}

mobj vector_ref(mobj v, mobj i) {
    mobj len = Mfixnum(minim_vector_len(v));
    if (minim_negativep(i) || fix_ge(i, len)) 
        error1("vector_ref", "index is negative", i);
    return minim_vector_ref(v, minim_fixnum(i));
}

mobj vector_set(mobj v, mobj i, mobj o) {
    mobj len = Mfixnum(minim_vector_len(v));
    if (minim_negativep(i) || fix_ge(i, len)) 
        error1("vector_set", "index is negative", i);
    minim_vector_ref(v, minim_fixnum(i)) = o;
    return minim_void;
}

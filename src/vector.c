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

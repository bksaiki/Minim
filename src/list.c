// list.c: pairs and lists

#include "minim.h"

size_t list_length(mobj o) {
    size_t l = 0;
    while (!minim_nullp(o)) {
        o = minim_cdr(o);
        ++l;
    }
    return l;
}

mobj list_reverse(mobj o) {
    mobj r = minim_null;
    while (!minim_nullp(o)) {
        r = Mcons(minim_car(o), r);
        o = minim_cdr(o);
    }
    return r;
}

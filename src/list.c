// list.c: pairs and lists

#include "minim.h"

int listp(mobj l) {
    while (minim_consp(l)) l = minim_cdr(l);
    return minim_nullp(l);
}

size_t list_length(mobj l) {
    size_t i = 0;
    for (; !minim_nullp(l); l = minim_cdr(l)) i++;
    return i;
}

mobj list_reverse(mobj l) {
    mobj r = minim_null;
    for (; !minim_nullp(l); l = minim_cdr(l))
        r = Mcons(minim_car(l), r);
    return r;
}

mobj list_member(mobj x, mobj l) {
    for (; !minim_nullp(l); l = minim_cdr(l)) {
        if (minim_equalp(minim_car(x), l))
            return minim_true;
    }
    return minim_false;
}

mobj list_append(mobj x, mobj y) {
    
}

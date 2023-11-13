// list.c: pairs and lists

#include "minim.h"

int listp(mobj l) {
    while (minim_consp(l)) l = minim_cdr(l);
    return minim_nullp(l);
}

size_t list_length(mobj l) {
    size_t i = 0;
    for_each(l, i++);
    return i;
}

mobj make_list(mobj k, mobj i) {
    mobj r = minim_null;
    size_t l = minim_fixnum(k);
    while (l > 0) {
        r = Mcons(i, r);
        l--;
    }

    return r;
}

mobj list_reverse(mobj l) {
    mobj r = minim_null;
    for_each(l, r = Mcons(minim_car(l), r));
    return r;
}

mobj list_ref(mobj l, mobj i) {
    mobj k = Mfixnum(minim_fixnum(i));
    if (minim_negativep(k))
        error1("list_ref", "index is negative", i);

    for_each(l,
        if (minim_zerop(k)) return minim_car(l);
        minim_fixnum(k)--;
    );

    error1("list_ref", "index out of bounds", i);
}

mobj list_member(mobj x, mobj l) {
    for_each(l, if (minim_equalp(minim_car(x), l)) return minim_true)
    return minim_false;
}

mobj list_append(mobj x, mobj y) {
    mobj l, t, i;

    if (minim_nullp(x))
        return y;

    l = t = Mcons(minim_car(x), NULL);
    i = minim_cdr(x);
    for_each(i,
        minim_cdr(t) = Mcons(minim_car(i), NULL);
        t = minim_cdr(t);
    )

    minim_cdr(t) = y;
    return l;
}

mobj list_assoc(mobj k, mobj l) {
    for_each(l,
        if (minim_equalp(minim_caar(l), k))
            return minim_car(l);
    );

    return minim_false;
}

mobj list_assq(mobj k, mobj l) {
    for_each(l,
        if (minim_eqp(minim_caar(l), k))
            return minim_car(l);
    );

    return minim_false;
}

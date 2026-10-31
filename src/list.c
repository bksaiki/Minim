// list.c: list and pair primitives 

#include "minim.h"

obj Mset_car(obj x, obj y) {
    Mcar(x) = y;
    return Mvoid;
}

obj Mset_cdr(obj x, obj y) {
    Mcdr(x) = y;
    return Mvoid;
}

int Mlistp(obj x) {
    for (; Mconsp(x); x = Mcdr(x));
    return Mnullp(x);
}

iptr list_length(obj x) {
    iptr l;
    for (l = 0; !Mnullp(x); x = Mcdr(x), ++l);
    return l;
}

obj Mmake_list(obj n, obj init) {
    obj xs;
    iptr i;
    
    xs = Mnull;
    for (i = Mfixnum_value(n); i >= 0; i--) {
        xs = Mcons(init, xs);
    }

    return xs;
}

obj Mlength(obj x) {
    return Mfixnum(list_length(x));
}

obj Mreverse(obj x) {
    obj hd = Mnull;
    while (!Mnullp(x)) {
        hd = Mcons(Mcar(x), hd);
        x = Mcdr(x);
    }

    return hd;
}

obj Mappend(obj x, obj y) {
    obj hd, tl;

    if (Mnullp(x)) {
        return y;
    } else if (Mnullp(y)) {
        return x;
    } else {
        hd = tl = Mlist1(Mcar(x));
        for (x = Mcdr(x); !Mnullp(x); x = Mcdr(x)) {
            Mcdr(tl) = Mlist1(Mcar(x));
            tl = Mcdr(tl);
        }

        Mcdr(tl) = y;
        return hd;
    }
}

obj list_tail(obj x, iptr i) {
    for (; i > 0; i--, x = Mcdr(x));
    return x;
}

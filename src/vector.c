// vector.c: primitive for vectors

#include "minim.h"

obj list_to_vector(obj x) {
    obj v;
    uptr len, i;

    if (Mnullp(x)) {
        return Memptyvec;
    } else {
        len = list_length(x);
        v = Mvector(len, NULL);

        i = 0;
        for (; Mconsp(x); x = Mcdr(x)) {
            Mvector_ref(v, i) = Mcar(x);
            i++;
        }

        return v;
    }
}

obj vector_to_list(obj v) {
    obj hd, tl;
    uptr len, i;

    len = Mvector_len(v);
    if (len == 0) {
        return Mnull;
    } else {
        hd = tl = Mlist1(Mvector_ref(v, 0));
        for (i = 1; i < len; i++) {
            Mcdr(tl) = Mlist1(Mvector_ref(v, i));
            tl = Mcdr(tl);
        }

        return hd;
    }
}

// obj.c: objects

#include "minim.h"

int Meqp(obj x, obj y) {
    if (x == y) {
        return 1;
    } else if (obj_type(x) != obj_type(y)) {
        return 0;
    } else {
        switch (obj_type(x)) {
        // fixnums (compare numerical values)
        case FIXNUM_OBJ_TYPE:
            return Mfixnum_value(x) == Mfixnum_value(y);
        default:
            return 0;
        }
    }
}

int Mequalp(obj x, obj y) {
    uptr i;

    if (Meqp(x, y)) {
        return 1;
    } else if (obj_type(x) != obj_type(y)) {
        return 0;
    } else {
        switch (obj_type(x)) {
        case STRING_OBJ_TYPE:
            return strcmp(Mstring_value(x), Mstring_value(y)) == 0;
        case CONS_OBJ_TYPE:
            return Mequalp(Mcar(x), Mcar(y)) && Mequalp(Mcdr(x), Mcdr(y));
        case VECTOR_OBJ_TYPE:
            if (Mvector_len(x) != Mvector_len(y))
                return 0;
            
            for (i = 0; i < Mvector_len(x); i++) {
                if (!Mequalp(Mvector_ref(x, i), Mvector_ref(y, i)))
                    return 0;
            }

            return 1;

        default:
            return 0;
        }
    }
}

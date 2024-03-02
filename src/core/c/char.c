/*
    Characters
*/

#include "../minim.h"

mobj Mchar(mchar c) {
    mobj o = GC_alloc_atomic(sizeof(minim_char_size));
    minim_type(o) = MINIM_OBJ_CHAR;
    minim_char(o) = c;
    return o;
}

//
//  Primitives
//

mobj charp_proc(mobj x) {
    return minim_charp(x) ? minim_true : minim_false;
}

mobj char_to_integer(mobj x) {
    return Mfixnum(minim_char(x));
}

mobj integer_to_char(mobj x) {
    return Mchar(minim_fixnum(x));
}

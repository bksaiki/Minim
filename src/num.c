// num.c: numbers

#include "minim.h"

mobj fix_neg(mobj x) {
    return Mfixnum(-minim_fixnum(x));
}

mobj fix_add(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) + minim_fixnum(y));
}

mobj fix_sub(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) - minim_fixnum(y));
}

mobj fix_mul(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) * minim_fixnum(y));
}

mobj fix_div(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) / minim_fixnum(y));
}

mobj fix_rem(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) % minim_fixnum(y));
}

mobj fix_eq(mobj x, mobj y) {
    return minim_fixnum(x) == minim_fixnum(y) ? minim_true : minim_false;
}

mobj fix_lt(mobj x, mobj y) {
    return minim_fixnum(x) < minim_fixnum(y) ? minim_true : minim_false;
}

mobj fix_gt(mobj x, mobj y) {
    return minim_fixnum(x) > minim_fixnum(y) ? minim_true : minim_false;
}

mobj fix_le(mobj x, mobj y) {
    return minim_fixnum(x) <= minim_fixnum(y) ? minim_true : minim_false;
}

mobj fix_ge(mobj x, mobj y) {
    return minim_fixnum(x) >= minim_fixnum(y) ? minim_true : minim_false;
}

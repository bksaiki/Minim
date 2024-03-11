/*
    Numbers
*/

#include "../minim.h"

mobj Mfixnum(long v) {
    mobj o = GC_alloc_atomic(minim_fixnum_size);
    minim_type(o) = MINIM_OBJ_FIXNUM;
    minim_fixnum(o) = v;
    return o;
}

//
//  Primitives
//

mobj fixnump_proc(mobj x) {
    return minim_fixnump(x) ? minim_true : minim_false;
}

mobj fx_neg(mobj x) {
    // (-> integer integer)
    return Mfixnum(-minim_fixnum(x));
}

mobj fx2_add(mobj x, mobj y) {
    // (-> integer integer integer)
    return Mfixnum(minim_fixnum(x) + minim_fixnum(y));
}

mobj fx2_sub(mobj x, mobj y) {
    // (-> integer integer integer)
    return Mfixnum(minim_fixnum(x) - minim_fixnum(y));
}

mobj fx2_mul(mobj x, mobj y) {
    // (-> integer integer integer)
    return Mfixnum(minim_fixnum(x) * minim_fixnum(y));
}

mobj fx2_div(mobj x, mobj y) {
    // (-> integer integer integer)
    return Mfixnum(minim_fixnum(x) / minim_fixnum(y));
}

mobj fx_remainder(mobj x, mobj y) {
    // (-> integer integer integer)
    return Mfixnum(minim_fixnum(x) % minim_fixnum(y));
}

mobj fx_modulo(mobj x, mobj y) {
    // (-> integer integer integer)
    mfixnum xv = minim_fixnum(x);
    mfixnum yv = minim_fixnum(y);
    mfixnum z = xv % yv;
    if ((xv < 0) != (yv < 0))
        z += yv;
    return Mfixnum(z);
}

mobj fx2_eq(mobj x, mobj y) {
    // (-> integer integer bool)
    return (minim_fixnum(x) == minim_fixnum(y)) ? minim_true : minim_false;
}

mobj fx2_gt(mobj x, mobj y) {
    // (-> integer integer bool)
    return (minim_fixnum(x) > minim_fixnum(y)) ? minim_true : minim_false;
}

mobj fx2_lt(mobj x, mobj y) {
    // (-> integer integer bool)
    return (minim_fixnum(x) < minim_fixnum(y)) ? minim_true : minim_false;
}

mobj fx2_ge(mobj x, mobj y) {
    // (-> integer integer bool)
    return (minim_fixnum(x) >= minim_fixnum(y)) ? minim_true : minim_false;
}

mobj fx2_le(mobj x, mobj y) {
    // (-> integer integer bool)
    return (minim_fixnum(x) <= minim_fixnum(y)) ? minim_true : minim_false;
}

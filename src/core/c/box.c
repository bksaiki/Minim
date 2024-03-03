/*
    Boxes
*/

#include "../minim.h"

mobj Mbox(mobj x) {
    mobj o = GC_alloc(minim_box_size);
    minim_type(o) = MINIM_OBJ_BOX;
    minim_unbox(o) = x;
    return o;
}

//
//  Primitives
//

mobj boxp_proc(mobj x) {
    // (-> any boolean)
    return minim_boxp(x) ? minim_true : minim_false;
}

mobj box_proc(mobj x) {
    // (-> any box)
    return Mbox(x);
}

mobj unbox_proc(mobj x) {
    // (-> box any)
    return minim_unbox(x);
}

mobj box_set_proc(mobj x, mobj v) {
    // (-> box any void)
    minim_unbox(x) = v;
    return minim_void;
}

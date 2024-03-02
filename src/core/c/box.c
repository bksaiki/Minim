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

mobj is_box_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_boxp(args[0]) ? minim_true : minim_false;
}

mobj box_proc(int argc, mobj *args) {
    // (-> any box)
    return Mbox(args[0]);
}

mobj unbox_proc(int argc, mobj *args) {
    // (-> box any)
    if (!minim_boxp(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    return minim_unbox(args[0]);
}

mobj box_set_proc(int argc, mobj *args) {
    // (-> box any void)
    if (!minim_boxp(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    minim_unbox(args[0]) = args[1];
    return minim_void;
}

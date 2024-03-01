/*
    Boxes
*/

#include "../minim.h"

mobj *make_box(mobj *x) {
    minim_box_object *o = GC_alloc(sizeof(minim_box_object));
    o->type = MINIM_BOX_TYPE;
    o->o = x;
    return ((mobj *) o);
}

//
//  Primitives
//

mobj *is_box_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_is_box(args[0]) ? minim_true : minim_false;
}

mobj *box_proc(int argc, mobj **args) {
    // (-> any box)
    return make_box(args[0]);
}

mobj *unbox_proc(int argc, mobj **args) {
    // (-> box any)
    if (!minim_is_box(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    return minim_box_contents(args[0]);
}

mobj *box_set_proc(int argc, mobj **args) {
    // (-> box any void)
    if (!minim_is_box(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    minim_box_contents(args[0]) = args[1];
    return minim_void;
}

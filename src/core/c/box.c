/*
    Boxes
*/

#include "../minim.h"

minim_object *make_box(minim_object *x) {
    minim_box_object *o = GC_alloc(sizeof(minim_box_object));
    o->type = MINIM_BOX_TYPE;
    o->o = x;
    return ((minim_object *) o);
}

//
//  Primitives
//

minim_object *is_box_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_box(args[0]) ? minim_true : minim_false;
}

minim_object *box_proc(int argc, minim_object **args) {
    // (-> any box)
    return make_box(args[0]);
}

minim_object *unbox_proc(int argc, minim_object **args) {
    // (-> box any)
    if (!minim_is_box(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    return minim_box_contents(args[0]);
}

minim_object *box_set_proc(int argc, minim_object **args) {
    // (-> box any void)
    if (!minim_is_box(args[0]))
        bad_type_exn("unbox", "box?", args[0]);

    minim_box_contents(args[0]) = args[1];
    return minim_void;
}

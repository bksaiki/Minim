/*
    Characters
*/

#include "../minim.h"

mobj *Mchar(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
    return ((mobj *) o);
}

//
//  Primitives
//

mobj *is_char_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_is_char(args[0]) ? minim_true : minim_false;
}

mobj *char_to_integer_proc(int argc, mobj **args) {
    // (-> char integer)
    mobj *o = args[0];
    if (!minim_is_char(o))
        bad_type_exn("char->integer", "char?", o);
    return Mfixnum(minim_char(o));
}

mobj *integer_to_char_proc(int argc, mobj **args) {
    // (-> integer char)
    mobj *o = args[0];
    if (!minim_is_fixnum(o))
        bad_type_exn("integer->char", "integer?", o);
    return Mchar(minim_fixnum(args[0]));
}

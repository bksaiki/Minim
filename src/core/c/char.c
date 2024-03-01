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

mobj *is_char_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_charp(args[0]) ? minim_true : minim_false;
}

mobj *char_to_integer_proc(int argc, mobj *args) {
    // (-> char integer)
    mobj *o = args[0];
    if (!minim_charp(o))
        bad_type_exn("char->integer", "char?", o);
    return Mfixnum(minim_char(o));
}

mobj *integer_to_char_proc(int argc, mobj *args) {
    // (-> integer char)
    mobj *o = args[0];
    if (!minim_fixnump(o))
        bad_type_exn("integer->char", "integer?", o);
    return Mchar(minim_fixnum(args[0]));
}

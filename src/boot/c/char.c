/*
    Characters
*/

#include "../minim.h"

minim_object *make_char(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
    return ((minim_object *) o);
}

//
//  Primitives
//

minim_object *is_char_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_char(args[0]) ? minim_true : minim_false;
}

minim_object *char_to_integer_proc(int argc, minim_object **args) {
    // (-> char integer)
    minim_object *o = args[0];
    if (!minim_is_char(o))
        bad_type_exn("char->integer", "char?", o);
    return make_fixnum(minim_char(o));
}

minim_object *integer_to_char_proc(int argc, minim_object **args) {
    // (-> integer char)
    minim_object *o = args[0];
    if (!minim_is_fixnum(o))
        bad_type_exn("integer->char", "integer?", o);
    return make_char(minim_fixnum(args[0]));
}

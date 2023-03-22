/*
    Records
*/

#include "../minim.h"

minim_object *make_record(minim_object *rtd, int fieldc) {
    minim_record_object *o = GC_alloc(sizeof(minim_record_object));
    o->type = MINIM_RECORD_TYPE;
    o->rtd = rtd;
    o->fields = GC_calloc(fieldc, sizeof(minim_object*));
    o->fieldc = fieldc;
    return ((minim_object *) o);
}

//
//
//

minim_object *is_record_proc(int argc, minim_object **args) {
    // (-> any boolean)
    minim_object *rtd;

    if (!minim_is_record(args[0]))
        return minim_false;

    rtd = minim_record_rtd(args[0]);
    return record_rtd_opaque(rtd) != minim_false;
}

minim_object *is_record_rtd_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_record_rtd(args[0]) ? minim_true : minim_false;
}

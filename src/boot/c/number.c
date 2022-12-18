/*
    Numbers
*/

#include "../minim.h"

minim_object *make_fixnum(long v) {
    minim_fixnum_object *o = GC_alloc(sizeof(minim_fixnum_object));
    o->type = MINIM_FIXNUM_TYPE;
    o->value = v;
    return ((minim_object *) o);
}

//
//  Primitives
//

minim_object *is_fixnum_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_fixnum(minim_car(args)) ? minim_true : minim_false;
}

minim_object *add_proc(minim_object *args) {
    // (-> integer ... integer)
    long result = 0;
 
    while (!minim_is_null(args)) {
        if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("+", "integer?", minim_car(args));
        result += minim_fixnum(minim_car(args));
        args = minim_cdr(args);
    }

    return make_fixnum(result);
}

minim_object *sub_proc(minim_object *args) {
    // (-> integer integer ... integer)
    long result;

    if (!minim_is_fixnum(minim_car(args)))
        bad_type_exn("-", "integer?", minim_car(args));
    
    if (minim_is_null(minim_cdr(args))) {
        result = -(minim_fixnum(minim_car(args)));
    } else {
        result = minim_fixnum(minim_car(args));
        args = minim_cdr(args);
        while (!minim_is_null(args)) {
            if (!minim_is_fixnum(minim_car(args)))
                bad_type_exn("-", "integer?", minim_car(args));
            result -= minim_fixnum(minim_car(args));
            args = minim_cdr(args);
        }
    }

    return make_fixnum(result);
}

minim_object *mul_proc(minim_object *args) {
    // (-> integer ... integer)
    long result = 1;

    while (!minim_is_null(args)) {
        if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("*", "integer?", minim_car(args));
        result *= minim_fixnum(minim_car(args));
        args = minim_cdr(args);
    }

    return make_fixnum(result);
}

minim_object *div_proc(minim_object *args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("/", "integer?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("/", "integer?", minim_cadr(args));

    return make_fixnum(minim_fixnum(minim_car(args)) /
                       minim_fixnum(minim_cadr(args)));
}

minim_object *remainder_proc(minim_object *args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("remainder", "integer?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("remainder", "integer?", minim_cadr(args));

    return make_fixnum(minim_fixnum(minim_car(args)) %
                       minim_fixnum(minim_cadr(args)));
}

minim_object *modulo_proc(minim_object *args) {
    // (-> integer integer integer)
    long result;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("modulo", "integer?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("modulo", "integer?", minim_cadr(args));

    result = minim_fixnum(minim_car(args)) % minim_fixnum(minim_cadr(args));
    if ((minim_fixnum(minim_car(args)) < 0) != (minim_fixnum(minim_cadr(args)) < 0))
        result += minim_fixnum(minim_cadr(args));
    return make_fixnum(result);
}

minim_object *number_eq_proc(minim_object *args) {
    // (-> number number boolean)
    long a, b;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("=", "number?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("=", "number?", minim_cadr(args));

    a = minim_fixnum(minim_car(args));
    b = minim_fixnum(minim_cadr(args));
    return a == b ? minim_true : minim_false;
}

minim_object *number_ge_proc(minim_object *args) { 
    // (-> number number boolean)
    long a, b;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn(">=", "number?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn(">=", "number?", minim_cadr(args));

    a = minim_fixnum(minim_car(args));
    b = minim_fixnum(minim_cadr(args));
    return a >= b ? minim_true : minim_false;
}

minim_object *number_le_proc(minim_object *args) { 
    // (-> number number boolean)
    long a, b;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("<=", "number?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("<=", "number?", minim_cadr(args));

    a = minim_fixnum(minim_car(args));
    b = minim_fixnum(minim_cadr(args));
    return a <= b ? minim_true : minim_false;
}

minim_object *number_gt_proc(minim_object *args) { 
    // (-> number number boolean)
    long a, b;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn(">", "number?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn(">", "number?", minim_cadr(args));

    a = minim_fixnum(minim_car(args));
    b = minim_fixnum(minim_cadr(args));
    return a > b ? minim_true : minim_false;
}

minim_object *number_lt_proc(minim_object *args) { 
    // (-> number number boolean)
    long a, b;

    if (!minim_is_fixnum(minim_car(args)))
            bad_type_exn("<", "number?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)))
            bad_type_exn("<", "number?", minim_cadr(args));

    a = minim_fixnum(minim_car(args));
    b = minim_fixnum(minim_cadr(args));
    return a < b ? minim_true : minim_false;
}

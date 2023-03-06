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

minim_object *is_fixnum_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_fixnum(args[0]) ? minim_true : minim_false;
}

minim_object *add_proc(int argc, minim_object **args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 0;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("+", "integer?", args[i]);
        result += minim_fixnum(args[i]);
    }

    return make_fixnum(result);
}

minim_object *sub_proc(int argc, minim_object **args) {
    // (-> integer integer ... integer)
    long result;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn("-", "integer?", args[0]);
    
    if (argc == 1) {
        result = -(minim_fixnum(args[0]));
    } else {
        result = minim_fixnum(args[0]);
        for (i = 1; i < argc; ++i) {
            if (!minim_is_fixnum(args[i]))
                bad_type_exn("-", "integer?", args[i]);
            result -= minim_fixnum(args[i]);
        }
    }

    return make_fixnum(result);
}

minim_object *mul_proc(int argc, minim_object **args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 1;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("*", "integer?", args[i]);
        result *= minim_fixnum(args[i]);
    }

    return make_fixnum(result);
}

minim_object *div_proc(int argc, minim_object **args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(args[0]))
            bad_type_exn("/", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("/", "integer?", args[1]);

    return make_fixnum(minim_fixnum(args[0]) / minim_fixnum(args[1]));
}

minim_object *remainder_proc(int argc, minim_object **args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(args[0]))
            bad_type_exn("remainder", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("remainder", "integer?", args[1]);

    return make_fixnum(minim_fixnum(args[0]) % minim_fixnum(args[1]));
}

minim_object *modulo_proc(int argc, minim_object **args) {
    // (-> integer integer integer)
    long result;

    if (!minim_is_fixnum(args[0]))
            bad_type_exn("modulo", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("modulo", "integer?", args[1]);

    result = minim_fixnum(args[0]) % minim_fixnum(args[1]);
    if ((minim_fixnum(args[0]) < 0) != (minim_fixnum(args[1]) < 0))
        result += minim_fixnum(args[1]);
    return make_fixnum(result);
}

minim_object *number_eq_proc(int argc, minim_object **args) {
    // (-> number number number ... boolean)
    long x0;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn("=", "number?", args[0]);
    x0 = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("=", "number?", args[i]);
        if (x0 != minim_fixnum(args[i]))
            return minim_false;
    }

    return minim_true;
}

minim_object *number_ge_proc(int argc, minim_object **args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn(">=", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn(">=", "number?", args[i]);
        if (xi < minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

minim_object *number_le_proc(int argc, minim_object **args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn("<=", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("<=", "number?", args[i]);
        if (xi > minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

minim_object *number_gt_proc(int argc, minim_object **args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn(">", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn(">", "number?", args[i]);
        if (xi <= minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

minim_object *number_lt_proc(int argc, minim_object **args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_is_fixnum(args[0]))
        bad_type_exn("<", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("<", "number?", args[i]);
        if (xi >= minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

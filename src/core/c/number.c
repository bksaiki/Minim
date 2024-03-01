/*
    Numbers
*/

#include "../minim.h"

mobj *Mfixnum(long v) {
    minim_fixnum_object *o = GC_alloc(sizeof(minim_fixnum_object));
    o->type = MINIM_FIXNUM_TYPE;
    o->value = v;
    return ((mobj *) o);
}

//
//  Primitives
//

mobj *is_fixnum_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_is_fixnum(args[0]) ? minim_true : minim_false;
}

mobj *add_proc(int argc, mobj **args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 0;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("+", "integer?", args[i]);
        result += minim_fixnum(args[i]);
    }

    return Mfixnum(result);
}

mobj *sub_proc(int argc, mobj **args) {
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

    return Mfixnum(result);
}

mobj *mul_proc(int argc, mobj **args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 1;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_fixnum(args[i]))
            bad_type_exn("*", "integer?", args[i]);
        result *= minim_fixnum(args[i]);
    }

    return Mfixnum(result);
}

mobj *div_proc(int argc, mobj **args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(args[0]))
            bad_type_exn("/", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("/", "integer?", args[1]);

    return Mfixnum(minim_fixnum(args[0]) / minim_fixnum(args[1]));
}

mobj *remainder_proc(int argc, mobj **args) {
    // (-> integer integer integer)
    if (!minim_is_fixnum(args[0]))
            bad_type_exn("remainder", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("remainder", "integer?", args[1]);

    return Mfixnum(minim_fixnum(args[0]) % minim_fixnum(args[1]));
}

mobj *modulo_proc(int argc, mobj **args) {
    // (-> integer integer integer)
    long result;

    if (!minim_is_fixnum(args[0]))
            bad_type_exn("modulo", "integer?", args[0]);
    if (!minim_is_fixnum(args[1]))
            bad_type_exn("modulo", "integer?", args[1]);

    result = minim_fixnum(args[0]) % minim_fixnum(args[1]);
    if ((minim_fixnum(args[0]) < 0) != (minim_fixnum(args[1]) < 0))
        result += minim_fixnum(args[1]);
    return Mfixnum(result);
}

mobj *number_eq_proc(int argc, mobj **args) {
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

mobj *number_ge_proc(int argc, mobj **args) { 
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

mobj *number_le_proc(int argc, mobj **args) { 
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

mobj *number_gt_proc(int argc, mobj **args) { 
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

mobj *number_lt_proc(int argc, mobj **args) { 
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

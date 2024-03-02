/*
    Numbers
*/

#include "../minim.h"

mobj Mfixnum(long v) {
    mobj o = GC_alloc_atomic(minim_fixnum_size);
    minim_type(o) = MINIM_OBJ_FIXNUM;
    minim_fixnum(o) = v;
    return o;
}

//
//  Primitives
//

mobj fixnump_proc(mobj x) {
    return minim_fixnump(x) ? minim_true : minim_false;
}

mobj fx2_add(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) + minim_fixnum(y));
}

mobj fx2_sub(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) - minim_fixnum(y));
}

mobj fx2_mul(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) * minim_fixnum(y));
}

mobj fx2_div(mobj x, mobj y) {
    return Mfixnum(minim_fixnum(x) / minim_fixnum(y));
}

mobj add_proc(int argc, mobj *args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 0;
    for (i = 0; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn("+", "integer?", args[i]);
        result += minim_fixnum(args[i]);
    }

    return Mfixnum(result);
}

mobj sub_proc(int argc, mobj *args) {
    // (-> integer integer ... integer)
    long result;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn("-", "integer?", args[0]);
    
    if (argc == 1) {
        result = -(minim_fixnum(args[0]));
    } else {
        result = minim_fixnum(args[0]);
        for (i = 1; i < argc; ++i) {
            if (!minim_fixnump(args[i]))
                bad_type_exn("-", "integer?", args[i]);
            result -= minim_fixnum(args[i]);
        }
    }

    return Mfixnum(result);
}

mobj mul_proc(int argc, mobj *args) {
    // (-> integer ... integer)
    long result;
    int i;
    
    result = 1;
    for (i = 0; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn("*", "integer?", args[i]);
        result *= minim_fixnum(args[i]);
    }

    return Mfixnum(result);
}

mobj div_proc(int argc, mobj *args) {
    // (-> integer integer integer)
    if (!minim_fixnump(args[0]))
            bad_type_exn("/", "integer?", args[0]);
    if (!minim_fixnump(args[1]))
            bad_type_exn("/", "integer?", args[1]);

    return Mfixnum(minim_fixnum(args[0]) / minim_fixnum(args[1]));
}

mobj remainder_proc(int argc, mobj *args) {
    // (-> integer integer integer)
    if (!minim_fixnump(args[0]))
            bad_type_exn("remainder", "integer?", args[0]);
    if (!minim_fixnump(args[1]))
            bad_type_exn("remainder", "integer?", args[1]);

    return Mfixnum(minim_fixnum(args[0]) % minim_fixnum(args[1]));
}

mobj modulo_proc(int argc, mobj *args) {
    // (-> integer integer integer)
    long result;

    if (!minim_fixnump(args[0]))
            bad_type_exn("modulo", "integer?", args[0]);
    if (!minim_fixnump(args[1]))
            bad_type_exn("modulo", "integer?", args[1]);

    result = minim_fixnum(args[0]) % minim_fixnum(args[1]);
    if ((minim_fixnum(args[0]) < 0) != (minim_fixnum(args[1]) < 0))
        result += minim_fixnum(args[1]);
    return Mfixnum(result);
}

mobj number_eq_proc(int argc, mobj *args) {
    // (-> number number number ... boolean)
    long x0;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn("=", "number?", args[0]);
    x0 = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn("=", "number?", args[i]);
        if (x0 != minim_fixnum(args[i]))
            return minim_false;
    }

    return minim_true;
}

mobj number_ge_proc(int argc, mobj *args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn(">=", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn(">=", "number?", args[i]);
        if (xi < minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

mobj number_le_proc(int argc, mobj *args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn("<=", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn("<=", "number?", args[i]);
        if (xi > minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

mobj number_gt_proc(int argc, mobj *args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn(">", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn(">", "number?", args[i]);
        if (xi <= minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

mobj number_lt_proc(int argc, mobj *args) { 
    // (-> number number number ... boolean)
    long xi;
    int i;

    if (!minim_fixnump(args[0]))
        bad_type_exn("<", "number?", args[0]);
    xi = minim_fixnum(args[0]);

    for (i = 1; i < argc; ++i) {
        if (!minim_fixnump(args[i]))
            bad_type_exn("<", "number?", args[i]);
        if (xi >= minim_fixnum(args[i]))
            return minim_false;
        xi = minim_fixnum(args[i]);
    }

    return minim_true;
}

/*
    Objects
*/

#include "../minim.h"

mobj quote_symbol;
mobj define_symbol;
mobj define_values_symbol;
mobj let_symbol;
mobj let_values_symbol;
mobj letrec_symbol;
mobj letrec_values_symbol;
mobj setb_symbol;
mobj if_symbol;
mobj lambda_symbol;
mobj begin_symbol;
mobj cond_symbol;
mobj else_symbol;
mobj and_symbol;
mobj or_symbol;
mobj quote_syntax_symbol;

mobj minim_null;
mobj minim_true;
mobj minim_false;
mobj minim_eof;
mobj minim_void;
mobj minim_empty_vec;
mobj minim_base_rtd;
mobj minim_values;

int minim_eqp(mobj a, mobj b) {
    if (a == b) {
        return 1;
    } else if (minim_type(a) != minim_type(b)) {
        return 0;
    } else {
        switch (minim_type(a)) {
        case MINIM_OBJ_CHAR:
            return minim_char(a) == minim_char(b);
        case MINIM_OBJ_FIXNUM:
            return minim_fixnum(a) == minim_fixnum(b);
        default:
            return 0;
        }
    }
}

static int minim_vector_equalp(mobj a, mobj b) {
    if (minim_vector_len(a) != minim_vector_len(b))
        return 0;

    for (long i = 0; i < minim_vector_len(a); ++i) {
        if (!minim_equalp(minim_vector_ref(a, i), minim_vector_ref(b, i)))
            return 0;
    }

    return 1;
}

int minim_equalp(mobj a, mobj b) {
    if (a == b) {
        return 1;
    } else if (minim_type(a) != minim_type(b)) {
        return 0;
    } else {
        switch (minim_type(a)) {
        case MINIM_OBJ_CHAR:
            return minim_char(a) == minim_char(b);
        case MINIM_OBJ_FIXNUM:
            return minim_fixnum(a) == minim_fixnum(b);
        case MINIM_OBJ_STRING:
            return strcmp(minim_string(a), minim_string(b)) == 0;
        case MINIM_OBJ_PAIR:
            return minim_equalp(minim_car(a), minim_car(b)) &&
                minim_equalp(minim_cdr(a), minim_cdr(b));
        case MINIM_OBJ_VECTOR:
            return minim_vector_equalp(a, b);
        case MINIM_OBJ_BOX:
            return minim_equalp(minim_unbox(a), minim_unbox(b));
        default:
            return 0;
        }
    }
}

//
//  Primitives
//

mobj nullp_proc(mobj x) {
    return minim_nullp(x) ? minim_true : minim_false;
}

mobj voidp_proc(mobj x) {
    return minim_voidp(x) ? minim_true : minim_false;
}

mobj eofp_proc(mobj x) {
    return minim_eofp(x) ? minim_true : minim_false;
}

mobj boolp_proc(mobj x) {
    return minim_boolp(x) ? minim_true : minim_false;
}

mobj not_proc(mobj x) {
    // (-> any boolean)
    return minim_falsep(x) ? minim_true : minim_false;   
}

mobj eq_proc(mobj x, mobj y) {
    // (-> any any boolean)
    return minim_eqp(x, y) ? minim_true : minim_false;
}

mobj equal_proc(mobj x, mobj y) {
    // (-> any any boolean)
    return minim_equalp(x, y) ? minim_true : minim_false;
}

mobj void_proc() {
    return minim_void;
}

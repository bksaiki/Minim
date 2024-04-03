/*
    Objects
*/

#include "../minim.h"

mobj begin_symbol;
mobj call_with_values_symbol;
mobj case_lambda_symbol;
mobj define_values_symbol;
mobj if_symbol;
mobj lambda_symbol;
mobj let_values_symbol;
mobj letrec_values_symbol;
mobj quote_symbol;
mobj quote_syntax_symbol;
mobj setb_symbol;
mobj values_symbol;

mobj mvcall_symbol;
mobj mvlet_symbol;
mobj mvvalues_symbol;

mobj apply_symbol;
mobj bind_symbol;
mobj bind_values_symbol;
mobj brancha_symbol;
mobj branchf_symbol;
mobj branchgt_symbol;
mobj branchlt_symbol;
mobj branchne_symbol;
mobj ccall_symbol;
mobj check_stack_symbol;
mobj clear_frame_symbol;
mobj do_apply_symbol;
mobj do_arity_error_symbol;
mobj do_eval_symbol;
mobj do_raise_symbol;
mobj do_rest_symbol;
mobj do_values_symbol;
mobj do_with_values_symbol;
mobj get_ac_symbol;
mobj get_arg_symbol;
mobj get_env_symbol;
mobj literal_symbol;
mobj lookup_symbol;
mobj tl_lookup_symbol;
mobj make_closure_symbol;
mobj make_env_symbol;
mobj pop_symbol;
mobj pop_env_symbol;
mobj push_symbol;
mobj push_env_symbol;
mobj rebind_symbol;
mobj ret_symbol;
mobj set_proc_symbol;
mobj save_cc_symbol;

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

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

mobj eq_proc_obj;
mobj equal_proc_obj;
mobj eq_hash_proc_obj;
mobj equal_hash_proc_obj;

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

int minim_equalp(mobj a, mobj b) {
    minim_thread *th;
    long stashc, i;
    int res;

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
            if (minim_vector_len(a) != minim_vector_len(b))
                return 0;
            for (i = 0; i < minim_vector_len(a); ++i) {
                if (!minim_equalp(minim_vector_ref(a, i), minim_vector_ref(b, i)))
                    return 0;
            }
            return 1;
        case MINIM_OBJ_BOX:
            return minim_equalp(minim_unbox(a), minim_unbox(b));
        case MINIM_OBJ_HASHTABLE:
            return hashtable_equalp(a, b);
        case MINIM_OBJ_RECORD:
            // TODO: Other Schemes make records return #f by default for equal?
            // while providing a way to specifiy an equality procedure for
            // records of the same type.
            // In this case, equal? would possibly need access to the
            // current environment. It's possible that setting the equality
            // procedure using anything other than a closure should throw
            // an error.
            //
            // Minim implements record equality in the following way:
            // By default, records of the same type are not `equal?`, but this behavior
            // can be overriden by modifying `current-record-equal-procedure`.
            if (record_valuep(a) && record_valuep(b)) {
                // Unsafe code to follow
                th = current_thread();
                stashc = stash_call_args();

                push_call_arg(a);
                push_call_arg(b);
                push_call_arg(env_lookup_var(global_env(th), intern("equal?")));
                res = !minim_falsep(call_with_args(record_equal_proc(th), global_env(th)));

                prepare_call_args(stashc);
                return res;
            } else {
                return 0;
            }

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

mobj eq_proc(int argc, mobj *args) {
    // (-> any any boolean)
    return minim_eqp(args[0], args[1]) ? minim_true : minim_false;
}

mobj equal_proc(int argc, mobj *args) {
    // (-> any any boolean)
    return minim_equalp(args[0], args[1]) ? minim_true : minim_false;
}

mobj void_proc(int argc, mobj *args) {
    // (-> void)
    return minim_void;
}

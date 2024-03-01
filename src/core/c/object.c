/*
    Objects
*/

#include "../minim.h"

mobj *quote_symbol;
mobj *define_symbol;
mobj *define_values_symbol;
mobj *let_symbol;
mobj *let_values_symbol;
mobj *letrec_symbol;
mobj *letrec_values_symbol;
mobj *setb_symbol;
mobj *if_symbol;
mobj *lambda_symbol;
mobj *begin_symbol;
mobj *cond_symbol;
mobj *else_symbol;
mobj *and_symbol;
mobj *or_symbol;
mobj *quote_syntax_symbol;

mobj *minim_null;
mobj *minim_empty_vec;
mobj *minim_true;
mobj *minim_false;
mobj *minim_eof;
mobj *minim_void;
mobj *minim_values;
mobj *minim_base_rtd;

mobj *eq_proc_obj;
mobj *equal_proc_obj;
mobj *eq_hash_proc_obj;
mobj *equal_hash_proc_obj;

int minim_is_eq(mobj *a, mobj *b) {
    if (a == b)
        return 1;

    if (a->type != b->type)
        return 0;

    switch (a->type) {
    case MINIM_FIXNUM_TYPE:
        return minim_fixnum(a) == minim_fixnum(b);

    case MINIM_CHAR_TYPE:
        return minim_char(a) == minim_char(b);

    default:
        return 0;
    }
}

int minim_is_equal(mobj *a, mobj *b) {
    minim_thread *th;
    long stashc, i;
    int res;

    if (minim_is_eq(a, b))
        return 1;

    if (a->type != b->type)
        return 0;

    switch (a->type)
    {
    case MINIM_STRING_TYPE:
        return strcmp(minim_string(a), minim_string(b)) == 0;

    case MINIM_PAIR_TYPE:
        return minim_is_equal(minim_car(a), minim_car(b)) &&
               minim_is_equal(minim_cdr(a), minim_cdr(b));

    case MINIM_VECTOR_TYPE:
        if (minim_vector_len(a) != minim_vector_len(b))
            return 0;

        for (i = 0; i < minim_vector_len(a); ++i) {
            if (!minim_is_equal(minim_vector_ref(a, i), minim_vector_ref(b, i)))
                return 0;
        }

        return 1;
    
    case MINIM_BOX_TYPE:
        return minim_is_equal(minim_box_contents(a), minim_box_contents(b));

    case MINIM_HASHTABLE_TYPE:
        return hashtable_is_equal(a, b);

    case MINIM_RECORD_TYPE:
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

        if (is_record_value(a) && is_record_value(b)) {
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
            return minim_is_eq(a, b);
        }

    default:
        return 0;
    }
}

//
//  Primitives
//

mobj *is_null_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_nullp(args[0]) ? minim_true : minim_false;
}

mobj *is_void_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_is_void(args[0]) ? minim_true : minim_false;
}

mobj *is_eof_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_is_eof(args[0]) ? minim_true : minim_false;
}

mobj *is_bool_proc(int argc, mobj *args) {
    // (-> any boolean)
    mobj *o = args[0];
    return (minim_truep(o) || minim_falsep(o)) ? minim_true : minim_false;
}

mobj *not_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_falsep(args[0]) ? minim_true : minim_false;   
}

mobj *eq_proc(int argc, mobj *args) {
    // (-> any any boolean)
    return minim_is_eq(args[0], args[1]) ? minim_true : minim_false;
}

mobj *equal_proc(int argc, mobj *args) {
    // (-> any any boolean)
    return minim_is_equal(args[0], args[1]) ? minim_true : minim_false;
}

mobj *void_proc(int argc, mobj *args) {
    // (-> void)
    return minim_void;
}

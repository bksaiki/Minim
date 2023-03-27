/*
    Objects
*/

#include "../minim.h"

minim_object *minim_null;
minim_object *minim_empty_vec;
minim_object *minim_true;
minim_object *minim_false;
minim_object *minim_eof;
minim_object *minim_void;
minim_object *minim_values;
minim_object *minim_base_rtd;

minim_object *quote_symbol;
minim_object *define_symbol;
minim_object *define_values_symbol;
minim_object *let_symbol;
minim_object *let_values_symbol;
minim_object *letrec_symbol;
minim_object *letrec_values_symbol;
minim_object *setb_symbol;
minim_object *if_symbol;
minim_object *lambda_symbol;
minim_object *begin_symbol;
minim_object *cond_symbol;
minim_object *else_symbol;
minim_object *and_symbol;
minim_object *or_symbol;
minim_object *quote_syntax_symbol;

int minim_is_eq(minim_object *a, minim_object *b) {
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

int minim_is_equal(minim_object *a, minim_object *b) {
    long i;

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
        // Any record type descriptor is only `equal?` to itself
        // Two record values are equal if and only if
        //  (i)  they share record types
        //  (ii) each of their fields are `equal?`
        //
        if (record_equal_proc(current_thread()) != minim_false &&
            is_record_value(a) &&
            is_record_value(b)) {
            // Unsafe code to follow
            clear_call_args();
            push_call_arg(a);
            push_call_arg(b);
            push_call_arg(env_lookup_var(global_env(current_thread()), intern("equal?")));
            return (call_with_args(record_equal_proc(current_thread()), global_env(current_thread())) == minim_false ? 0 : 1);
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

minim_object *is_null_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_null(args[0]) ? minim_true : minim_false;
}

minim_object *is_void_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_void(args[0]) ? minim_true : minim_false;
}

minim_object *is_eof_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_eof(args[0]) ? minim_true : minim_false;
}

minim_object *is_bool_proc(int argc, minim_object **args) {
    // (-> any boolean)
    minim_object *o = args[0];
    return (minim_is_true(o) || minim_is_false(o)) ? minim_true : minim_false;
}

minim_object *not_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_false(args[0]) ? minim_true : minim_false;   
}

minim_object *eq_proc(int argc, minim_object **args) {
    // (-> any any boolean)
    return minim_is_eq(args[0], args[1]) ? minim_true : minim_false;
}

minim_object *equal_proc(int argc, minim_object **args) {
    // (-> any any boolean)
    return minim_is_equal(args[0], args[1]) ? minim_true : minim_false;
}

minim_object *void_proc(int argc, minim_object **args) {
    // (-> void)
    return minim_void;
}

/*
    Objects
*/

#include "../minim.h"

minim_object *minim_null;
minim_object *minim_true;
minim_object *minim_false;
minim_object *minim_eof;
minim_object *minim_void;
minim_object *minim_values;

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
    
    default:
        return 0;
    }
}

//
//  Primitives
//

minim_object *is_null_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_null(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_void_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_void(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_eof_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_eof(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_bool_proc(minim_object *args) {
    // (-> any boolean)
    minim_object *o = minim_car(args);
    return (minim_is_true(o) || minim_is_false(o)) ? minim_true : minim_false;
}

minim_object *not_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_false(minim_car(args)) ? minim_true : minim_false;   
}

minim_object *eq_proc(minim_object *args) {
    // (-> any any boolean)
    return minim_is_eq(minim_car(args), minim_cadr(args)) ? minim_true : minim_false;
}

minim_object *equal_proc(minim_object *args) {
    // (-> any any boolean)
    return minim_is_equal(minim_car(args), minim_cadr(args)) ? minim_true : minim_false;
}

minim_object *void_proc(minim_object *args) {
    // (-> void)
    return minim_void;
}

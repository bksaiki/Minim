/*
    Strings
*/

#include "../minim.h"

//
//  Primitives
//

minim_object *make_string(const char *s) {
    minim_string_object *o = GC_alloc(sizeof(minim_string_object));
    int len = strlen(s);
    
    o->type = MINIM_STRING_TYPE;
    o->value = GC_alloc_atomic((len + 1) * sizeof(char));
    strncpy(o->value, s, len + 1);
    return ((minim_object *) o);
}

minim_object *make_string2(char *s) {
    minim_string_object *o = GC_alloc(sizeof(minim_string_object));
    
    o->type = MINIM_STRING_TYPE;
    o->value = s;
    return ((minim_object *) o);
}

minim_object *is_string_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_string(minim_car(args)) ? minim_true : minim_false;
}

minim_object *make_string_proc(minim_object *args) {
    // (-> non-negative-integer string)
    // (-> non-negative-integer char string)
    char *str;
    long len, i;
    int c;

    if (!minim_is_fixnum(minim_car(args)) || minim_fixnum(minim_car(args)) < 0)
        bad_type_exn("make-string", "non-negative-integer?", minim_car(args));
    len = minim_fixnum(minim_car(args));

    if (minim_is_null(minim_cdr(args))) {
        // 1st case
        c = 'a';
    } else {
        // 2nd case
        if (!minim_is_char(minim_cadr(args)))
            bad_type_exn("make-string", "char?", minim_cadr(args));
        c = minim_char(minim_cadr(args));
    }

    str = GC_alloc_atomic((len + 1) * sizeof(char));    
    for (i = 0; i < len; ++i)
        str[i] = c;
    str[i] = '\0';

    return make_string2(str);
}

minim_object *string_proc(minim_object *args) {
    // (-> char ... string)
    long len, i;
    char *str;

    len = list_length(args);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (i = 0; i < len; ++i) {
        if (!minim_is_char(minim_car(args)))
            bad_type_exn("make-string", "char?", minim_car(args));
        str[i] = minim_char(minim_car(args));
        args = minim_cdr(args);
    }
    str[i] = '\0';

    return make_string2(str);
}

minim_object *string_length_proc(minim_object *args) {
    // (-> string integer)
    minim_object *o = minim_car(args);
    if (!minim_is_string(o))
        bad_type_exn("string-length", "string?", o);
    return make_fixnum(strlen(minim_string(o)));
}

minim_object *string_ref_proc(minim_object *args) {
    // (-> string non-negative-integer char)
    char *str;
    long len, idx;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("string-ref", "string?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)) || minim_fixnum(minim_cadr(args)) < 0)
        bad_type_exn("string-ref", "non-negative-integer?", minim_cdar(args));

    str = minim_string(minim_car(args));
    idx = minim_fixnum(minim_cadr(args));
    len = strlen(str);

    if (idx >= len) {
        fprintf(stderr, "string-ref: index out of bounds\n");
        fprintf(stderr, " string: %s, length: %ld, index %ld\n", str, len, idx);
        minim_shutdown(1);
    }

    return make_char((int) str[idx]);
}

minim_object *string_set_proc(minim_object *args) {
    // (-> string non-negative-integer char void)
    char *str;
    long len, idx;
    char c;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("string-set!", "string?", minim_car(args));
    if (!minim_is_fixnum(minim_cadr(args)) || minim_fixnum(minim_cadr(args)) < 0)
        bad_type_exn("string-set!", "non-negative-integer?", minim_cdar(args));
    if (!minim_is_char(minim_car(minim_cddr(args))))
        bad_type_exn("string-set!", "char?", minim_car(minim_cddr(args)));

    str = minim_string(minim_car(args));
    idx = minim_fixnum(minim_cadr(args));
    c = minim_char(minim_car(minim_cddr(args)));
    len = strlen(str);

    if (idx >= len) {
        fprintf(stderr, "string-set!: index out of bounds\n");
        fprintf(stderr, " string: %s, length: %ld, index %ld\n", str, len, idx);
        minim_shutdown(1);
    }

    str[idx] = c;
    return minim_void;
}

minim_object *string_append_proc(minim_object *args) {
    minim_object *it;
    char *str;
    long len, i, j;

    len = 0; i = 0;
    for (it = args; !minim_is_null(it); it = minim_cdr(it)) {
        if (!minim_is_string(minim_car(it)))
            bad_type_exn("string-append", "string?", minim_car(it));
        len += strlen(minim_string(minim_car(it)));
    }

    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (it = args; !minim_is_null(it); it = minim_cdr(it)) {
        j = strlen(minim_string(minim_car(it)));
        strcpy(&str[i], minim_string(minim_car(it)));
        i += j;
    }

    str[len] = '\0';    
    return make_string2(str);
}

minim_object *number_to_string_proc(minim_object *args) {
    // (-> number string)
    minim_object *o;
    char buffer[30];

    o = minim_car(args);
    if (!minim_is_fixnum(o))
        bad_type_exn("number->string", "number?", o);
    sprintf(buffer, "%ld", minim_fixnum(o));
    return make_string(buffer);
}

minim_object *string_to_number_proc(minim_object *args) {
    // (-> string number)
    minim_object *o = minim_car(args);
    if (!minim_is_string(o))
        bad_type_exn("string->number", "string?", o);
    return make_fixnum(atoi(minim_string(o)));
}

minim_object *symbol_to_string_proc(minim_object *args) {
    // (-> symbol string)
    minim_object *o = minim_car(args);
    if (!minim_is_symbol(o))
        bad_type_exn("symbol->string", "symbol?", o);
    return make_string(minim_symbol(o));
}

minim_object *string_to_symbol_proc(minim_object *args) {
    // (-> string symbol)
    minim_object *o = minim_car(args);
    if (!minim_is_string(o))
        bad_type_exn("string->symbol", "string?", o);
    return intern(minim_string(o));
}

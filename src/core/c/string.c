/*
    Strings
*/

#include "../minim.h"

static void string_out_of_bounds_exn(const char *name, const char *str, long idx) {
    fprintf(stderr, "%s, index out of bounds\n", name);
    fprintf(stderr, " string: %s\n", str);
    fprintf(stderr, " length: %ld\n", strlen(str));
    fprintf(stderr, " index:  %ld\n", idx);
    minim_shutdown(1);
}

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

minim_object *is_string_proc(int argc, minim_object **args) {
    // (-> any boolean)
    return minim_is_string(args[0]) ? minim_true : minim_false;
}

minim_object *make_string_proc(int argc, minim_object **args) {
    // (-> non-negative-integer string)
    // (-> non-negative-integer char string)
    char *str;
    long len, i;
    int c;

    if (!minim_is_fixnum(args[0]) || minim_fixnum(args[0]) < 0)
        bad_type_exn("make-string", "non-negative-integer?", args[0]);
    len = minim_fixnum(args[0]);

    if (argc == 1) {
        // 1st case
        c = 'a';
    } else {
        // 2nd case
        if (!minim_is_char(args[1]))
            bad_type_exn("make-string", "char?", args[1]);
        c = minim_char(args[1]);
    }

    str = GC_alloc_atomic((len + 1) * sizeof(char));    
    for (i = 0; i < len; ++i)
        str[i] = c;
    str[i] = '\0';

    return make_string2(str);
}

minim_object *string_proc(int argc, minim_object **args) {
    // (-> char ... string)
    char *str;
    int i;

    str = GC_alloc_atomic((argc + 1) * sizeof(char));
    str[argc] = '\0';

    for (i = 0; i < argc; ++i) {
        if (!minim_is_char(args[i]))
            bad_type_exn("make-string", "char?", args[i]);
        str[i] = minim_char(args[i]);
    }

    return make_string2(str);
}

minim_object *string_length_proc(int argc, minim_object **args) {
    // (-> string integer)
    minim_object *o = args[0];
    if (!minim_is_string(o))
        bad_type_exn("string-length", "string?", o);
    return make_fixnum(strlen(minim_string(o)));
}

minim_object *string_ref_proc(int argc, minim_object **args) {
    // (-> string non-negative-integer char)
    char *str;
    long len, idx;

    if (!minim_is_string(args[0]))
        bad_type_exn("string-ref", "string?", args[0]);
    if (!minim_is_fixnum(args[1]) || minim_fixnum(args[1]) < 0)
        bad_type_exn("string-ref", "non-negative-integer?", minim_cdar(args));

    str = minim_string(args[0]);
    idx = minim_fixnum(args[1]);
    len = strlen(str);

    if (idx >= len)
        string_out_of_bounds_exn("string-ref", str, idx);

    return make_char((int) str[idx]);
}

minim_object *string_set_proc(int argc, minim_object **args) {
    // (-> string non-negative-integer char void)
    char *str;
    long len, idx;
    char c;

    if (!minim_is_string(args[0]))
        bad_type_exn("string-set!", "string?", args[0]);
    if (!minim_is_fixnum(args[1]) || minim_fixnum(args[1]) < 0)
        bad_type_exn("string-set!", "non-negative-integer?", minim_cdar(args));
    if (!minim_is_char(args[2]))
        bad_type_exn("string-set!", "char?", args[2]);

    str = minim_string(args[0]);
    idx = minim_fixnum(args[1]);
    c = minim_char(args[2]);
    len = strlen(str);

    if (idx >= len)
        string_out_of_bounds_exn("string-set!", str, idx);

    str[idx] = c;
    return minim_void;
}

minim_object *string_append_proc(int argc, minim_object **args) {
    // (-> string ... string)
    char *str;
    long len, j, l;
    int i;

    len = 0;
    for (i = 0; i < argc; ++i) {
        if (!minim_is_string(args[i]))
            bad_type_exn("string-append", "string?", args[i]);
        len += strlen(minim_string(args[i]));
    }

    str = GC_alloc_atomic((len + 1) * sizeof(char));
    str[len] = '\0';

    j = 0;
    for (i = 0; i < argc; ++i) {
        l = strlen(minim_string(args[i]));
        strcpy(&str[j], minim_string(args[i]));
        j += l;
    }
   
    return make_string2(str);
}

minim_object *number_to_string_proc(int argc, minim_object **args) {
    // (-> number string)
    minim_object *o;
    char buffer[30];

    o = args[0];
    if (!minim_is_fixnum(o))
        bad_type_exn("number->string", "number?", o);
    sprintf(buffer, "%ld", minim_fixnum(o));
    return make_string(buffer);
}

minim_object *string_to_number_proc(int argc, minim_object **args) {
    // (-> string number)
    minim_object *o = args[0];
    if (!minim_is_string(o))
        bad_type_exn("string->number", "string?", o);
    return make_fixnum(atoi(minim_string(o)));
}

minim_object *symbol_to_string_proc(int argc, minim_object **args) {
    // (-> symbol string)
    minim_object *o = args[0];
    if (!minim_is_symbol(o))
        bad_type_exn("symbol->string", "symbol?", o);
    return make_string(minim_symbol(o));
}

minim_object *string_to_symbol_proc(int argc, minim_object **args) {
    // (-> string symbol)
    minim_object *o = args[0];
    if (!minim_is_string(o))
        bad_type_exn("string->symbol", "string?", o);
    return intern(minim_string(o));
}

minim_object *format_proc(int argc, minim_object **args) {
    // (-> string any ... string)
    FILE *o;
    char *s;
    long len, i;
    
    if (!minim_is_string(args[0]))
        bad_type_exn("format", "string?", args[0]);

    o = tmpfile();
    minim_fprintf(o, minim_string(args[0]), argc - 1, &args[1], "format");

    len = ftell(o);
    s = GC_alloc_atomic((len + 1) * sizeof(char));
    s[len] = '\0';

    fseek(o, 0, SEEK_SET);
    for (i = 0; i < len; ++i)
        s[i] = fgetc(o);

    fclose(o);
    return make_string2(s);
}

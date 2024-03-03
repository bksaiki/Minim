/*
    Strings
*/

#include "../minim.h"

#define FIXNUM_STRING_LEN   24

mobj Mstring(const char *s) {
    mobj o;
    size_t len;

    o = GC_alloc(minim_string_size);
    len = strlen(s);
    minim_type(o) = MINIM_OBJ_STRING;
    minim_string(o) = GC_alloc_atomic((len + 1) * sizeof(char));
    strncpy(minim_string(o), s, len + 1);
    return o;
}

mobj Mstring2(char *s) {
    mobj o = GC_alloc(minim_string_size);
    minim_type(o) = MINIM_OBJ_STRING;
    minim_string(o) = s;
    return o;
}

mobj Mstring3(long len, mchar c) {
    mobj o = GC_alloc(minim_string_size);
    minim_type(o) = MINIM_OBJ_STRING;
    if (c == 0) {
        minim_string(o) = GC_calloc_atomic((len + 1), sizeof(char));
    } else {
        minim_string(o) = GC_alloc_atomic((len + 1) * sizeof(char));
        for (long i = 0; i < len; i++)
            minim_string(o)[i] = c;
        minim_string(o)[len] = '\0';
    }

    return o;
}

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

mobj stringp_proc(mobj x) {
    return minim_stringp(x) ? minim_true : minim_false;
}

mobj make_string(mobj len, mobj init) {
    // (-> non-negative-integer char string)
    return Mstring3(minim_fixnum(len), minim_char(init));
}

mobj string_length(mobj s) {
    // (-> string fixnum)
    return Mfixnum(strlen(minim_string(s)));
}

mobj string_ref(mobj s, mobj idx) {
    // (-> string fixnum any)
    return Mchar(minim_string(s)[minim_fixnum(idx)]);
}

mobj string_set(mobj s, mobj idx, mobj x) {
    // (-> string fixnum char void)
    minim_string(s)[minim_fixnum(idx)] = minim_char(x);
    return minim_void;
}

mobj number_to_string(mobj n) {
    // (-> number string)
    mobj s = Mstring3(FIXNUM_STRING_LEN, 0);
    sprintf(minim_string(s), "%ld", minim_fixnum(n));
    return s;
}

mobj string_to_number(mobj s) {
    // (-> string number)
    return Mfixnum(atoi(minim_string(s)));
}

mobj symbol_to_string(mobj s) {
    // (-> symbol string)
    return Mstring(minim_symbol(s));
}

mobj string_to_symbol(mobj s) {
    // (-> string symbol)
    return intern(minim_string(s));
}

mobj list_to_string(mobj xs) {
    // (-> (listof char) string)
    mobj s;
    long len, i;

    len = list_length(xs);
    s = Mstring3(len, 0);
    for (i = 0; i < len; i++) {
        minim_string(s)[i] = minim_char(minim_car(xs));
        xs = minim_cdr(xs);
    }

    minim_string(s)[len] = '\0';
    return s;
}

mobj string_to_list(mobj s) {
    // (-> string (listof char))
    mobj lst = minim_null;
    for (long i = strlen(minim_string(s)); i >= 0; i--)
        lst = Mcons(Mchar(minim_string(s)[i]), lst);
    return lst;
}

mobj format_proc(int argc, mobj *args) {
    // (-> string any ... string)
    FILE *o;
    char *s;
    long len, i;
    
    if (!minim_stringp(args[0]))
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
    return Mstring2(s);
}

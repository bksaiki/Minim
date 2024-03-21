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

mobj Mstring2(long len, mchar c) {
    mobj o = GC_alloc(minim_string_size);
    minim_type(o) = MINIM_OBJ_STRING;
    if (c == 0) {
        minim_string(o) = GC_calloc_atomic((len + 1), sizeof(char));
    } else {
        minim_string(o) = GC_alloc_atomic((len + 1) * sizeof(char));
        for (long i = 0; i < len; i++)
            minim_string(o)[i] = c;
    }

    minim_string(o)[len] = '\0';
    return o;
}

//
//  Primitives
//

mobj stringp_proc(mobj x) {
    return minim_stringp(x) ? minim_true : minim_false;
}

mobj make_string(mobj len, mobj init) {
    // (-> non-negative-integer char string)
    return Mstring2(minim_fixnum(len), minim_char(init));
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
    mobj s = Mstring2(FIXNUM_STRING_LEN, 0);
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
    s = Mstring2(len, 0);
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

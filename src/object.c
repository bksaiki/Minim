// object.c: low-level primitives on Scheme objects

#include "minim.h"

mobj minim_null;
mobj minim_true;
mobj minim_false;
mobj minim_eof;
mobj minim_void;

mobj Mchar(mchar c) {
    mobj o = GC_alloc(minim_char_size);
    minim_type(o) = MINIM_OBJ_CHAR;
    minim_char(o) = c;
    return o;
}

mobj Msymbol(const mchar *s) {
    size_t n = mstrlen(s);
    mchar *str = GC_alloc_atomic((n + 1) * sizeof(mchar));
    memcpy(str, s, n * sizeof(mchar));
    str[n] = 0;

    mobj o = GC_alloc(minim_symbol_size);
    minim_type(o) = MINIM_OBJ_SYMBOL;
    minim_symbol(o) = str;
    return o;
}

mobj Mstring(const char *s) {
    mobj o = GC_alloc(minim_string_size);
    minim_type(o) = MINIM_OBJ_STRING;
    minim_string(o) = mstr(s);
    return o;
}

mobj Mfixnum(mfixnum v) {
    mobj o = GC_alloc(minim_fixnum_size);
    minim_type(o) = MINIM_OBJ_FIXNUM;
    minim_fixnum(o) = v;
    return o;
}

mobj Mcons(mobj car, mobj cdr) {
    mobj o = GC_alloc(minim_cons_size);
    minim_type(o) = MINIM_OBJ_PAIR;
    minim_car(o) = car;
    minim_cdr(o) = cdr;
    return o;
}

mobj Mvector(msize n, mobj v) {
    mobj o = GC_alloc(minim_vector_size(n));
    minim_type(o) = MINIM_OBJ_VECTOR;
    minim_vector_len(o) = n;
    for (msize i = 0; i < n; i++)
        minim_vector_ref(o, i) = v;
    return o; 
}

mobj Mbox(mobj x) {
    mobj o = GC_alloc(minim_box_size);
    minim_type(o) = MINIM_OBJ_BOX;
    minim_unbox(o) = x;
    return o;
}

mobj Mport(FILE *f, mbyte flags) {
    mobj o = GC_alloc_atomic(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = flags;
    minim_port(o) = f;
    return o;
}

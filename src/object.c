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
    return Mstring2(mstr(s));
}

mobj Mstring2(mchar *s) {
    mobj o = GC_alloc(minim_string_size);
    minim_type(o) = MINIM_OBJ_STRING;
    minim_string(o) = s;
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

mobj Mclosure(mobj env, mobj code) {
    mobj o = GC_alloc(minim_closure_size);
    minim_type(o) = MINIM_OBJ_CLOSURE;
    minim_closure_env(o) = env;
    minim_closure_code(o) = code;
    return o;
}

static void gc_port_dtor(void *ptr, void *data) {
    mobj o = (mobj) ptr;
    if (minim_port_openp(o) &&
        minim_port(o) != stdout &&
        minim_port(o) != stdin &&
        minim_port(o) != stderr) {
        fclose(minim_port(o));
    }
}

mobj Mport(FILE *f, mbyte flags) {
    mobj o = GC_alloc_atomic(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = flags;
    minim_port(o) = f;
    GC_register_dtor(o, gc_port_dtor);
    return o;
}

int minim_eqp(mobj x, mobj y) {
    if (x == y) {
        return 1;
    } else if (minim_charp(x)) {
        return minim_charp(y) && minim_char(x) == minim_char(y);
    } else if (minim_fixnump(x)) {
        return minim_fixnum(y) && minim_fixnum(x) == minim_fixnum(y);
    } else {
        return 0;
    }
}

int minim_equalp(mobj x, mobj y) {
    if (minim_eqp(x, y)) {
        return 1;
    } else if (minim_stringp(x)) {
        return minim_stringp(y) && mstrcmp(minim_string(x), minim_string(y)) == 0;
    } else if (minim_consp(x)) {
        // TODO: cycle detection
        return minim_consp(y) && \
            minim_equalp(minim_car(x), minim_car(y)) && \
            minim_equalp(minim_cdr(x), minim_cdr(y));
    } else if (minim_vectorp(x)) {
        if (!minim_vectorp(y)) return 0;
        if (minim_vector_len(x) != minim_vector_len(y)) return 0;
        for (msize i = 0; i < minim_vector_len(x); i++) {
            if (!minim_equalp(minim_vector_ref(x, i), minim_vector_ref(y, i)))
                return 0;
        }
        return 1;
    } else {
        return 0;
    }
}

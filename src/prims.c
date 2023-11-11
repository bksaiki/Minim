// prims.c: primitive table

#include "minim.h"

// hashing from Chez Scheme

#define multiplier 3

static size_t symhash(const mchar* s) {
    size_t h, n;

    h = n = mstrlen(s);
    while (n--) h = h * multiplier + (mchar)*s++;
    return h % PRIM_TABLE_SIZE;
}

static mobj car_proc(mobj x) {
    return minim_car(x);
}

static mobj cdr_proc(mobj x) {
    return minim_cdr(x);
}

//
//  Public API
//

mobj lookup_prim(const mchar *name) {
    mobj p;
    size_t b;

    b = symhash(name);
    for (p = minim_vector_ref(M_glob.primlist, b); !minim_nullp(p); p = minim_cdr(p)) {
        if (mstrcmp(minim_string(minim_caar(p)), name) == 0) {
            return minim_cdar(p);
        }
    }

    return NULL;
}

void register_prim(const char *name, void *fn) {
    mobj s;
    size_t b;

    s = Mstring(name);
    if (lookup_prim(minim_string(s)) == 0) {
        b = symhash(minim_string(s));
        minim_vector_ref(M_glob.primlist, b) = Mcons(Mcons(s, fn), minim_vector_ref(M_glob.primlist, b));
    } else {
        error1("register_prim()", "duplicate identifier", s);
    }
}

void prim_table_init() {
    register_prim("cons", Mcons);
    register_prim("car", car_proc);
    register_prim("cdr", cdr_proc);
    register_prim("fxneg", fix_neg);
    register_prim("fx+", fix_add);
    register_prim("fx-", fix_sub);
    register_prim("fx*", fix_mul);
    register_prim("fx/", fix_div);
    register_prim("fxrem", fix_rem);

    register_prim("env_set", env_set);
    register_prim("env_get", env_get);
    register_prim("make_closure", Mclosure);
}

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

//
//  Primitives
//

static mobj nullp_proc(mobj x) {
    return minim_nullp(x) ? minim_true : minim_false;
}

static mobj consp_proc(mobj x) {
    return minim_consp(x) ? minim_true : minim_false;
}

static mobj fixnump_proc(mobj x) {
    return minim_fixnump(x) ? minim_true : minim_false;
}

static mobj car_proc(mobj x) {
    return minim_car(x);
}

static mobj cdr_proc(mobj x) {
    return minim_cdr(x);
}

static mobj write_proc(mobj x) {
    write_object(th_output_port(get_thread()), x);
    return minim_void;
}

static mobj newline_proc() {
    fputc('\n', minim_port(th_output_port(get_thread())));
    return minim_void;
}

static mobj flush_proc() {
    fflush(minim_port(th_output_port(get_thread())));
    return minim_void;
}

//
//  Exceptions
//

static mobj arity_exn(size_t actual, size_t expected) {
    fprintf(stderr, "arity mismatch\n");
    fprintf(stderr, " expected: %ld\n", expected);
    fprintf(stderr, " given: %ld\n", actual);
    fatal_exit();
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
    register_prim("null?", nullp_proc);
    register_prim("cons?", consp_proc);
    register_prim("cons", Mcons);
    register_prim("car", car_proc);
    register_prim("cdr", cdr_proc);

    register_prim("fx?", fixnump_proc);
    register_prim("fxneg", fix_neg);
    register_prim("fx+", fix_add);
    register_prim("fx-", fix_sub);
    register_prim("fx*", fix_mul);
    register_prim("fx/", fix_div);
    register_prim("fxrem", fix_rem);

    register_prim("fx=", fix_eq);
    register_prim("fx<", fix_lt);
    register_prim("fx>", fix_gt);
    register_prim("fx<=", fix_le);
    register_prim("fx>=", fix_ge);

    register_prim("write", write_proc);
    register_prim("newline", newline_proc);
    register_prim("flush-output", flush_proc);

    register_prim("env_extend", env_extend);
    register_prim("env_set", env_set);
    register_prim("env_define", env_define);
    register_prim("env_get", env_get);
    register_prim("make_closure", Mclosure);

    register_prim("arity_exn", arity_exn);
}

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

static mobj booleanp_proc(mobj x) {
    return minim_boolp(x) ? minim_true : minim_false;
}

static mobj nullp_proc(mobj x) {
    return minim_nullp(x) ? minim_true : minim_false;
}

static mobj consp_proc(mobj x) {
    return minim_consp(x) ? minim_true : minim_false;
}

static mobj charp_proc(mobj x) {
    return minim_charp(x) ? minim_true : minim_false;
}

static mobj stringp_proc(mobj x) {
    return minim_stringp(x) ? minim_true : minim_false;
}

static mobj symbolp_proc(mobj x) {
    return minim_symbolp(x) ? minim_true : minim_false;
}

static mobj fixnump_proc(mobj x) {
    return minim_fixnump(x) ? minim_true : minim_false;
}

static mobj vectorp_proc(mobj x) {
    return minim_vectorp(x) ? minim_true : minim_false;
}

static mobj procedurep_proc(mobj x) {
    return minim_closurep(x) ? minim_true : minim_false;
}

static mobj listp_proc(mobj x) {
    return listp(x) ? minim_true : minim_false;
}

static mobj not_proc(mobj x) {
    return minim_not(x);
}

static mobj car_proc(mobj x) {
    return minim_car(x);
}

static mobj cdr_proc(mobj x) {
    return minim_cdr(x);
}

static mobj length_proc(mobj x) {
    return Mfixnum(list_length(x));
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

static mobj error1_proc(mobj name, mobj why) {
    write_object(th_error_port(get_thread()), name);
    fprintf(minim_port(th_error_port(get_thread())), ": ");
    write_object(th_error_port(get_thread()), why);
    fprintf(minim_port(th_error_port(get_thread())), "\n");
    fatal_exit();
}

static mobj error2_proc(mobj name, mobj why, mobj what) {
    write_object(th_error_port(get_thread()), name);
    fprintf(minim_port(th_error_port(get_thread())), ": ");
    write_object(th_error_port(get_thread()), why);
    fprintf(minim_port(th_error_port(get_thread())), "\n what: ");
    write_object(th_error_port(get_thread()), what);
    fprintf(minim_port(th_error_port(get_thread())), "\n");
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
    // booleans
    register_prim("boolean?", booleanp_proc);
    register_prim("not", not_proc);
    // characters
    register_prim("char?", charp_proc);
    register_prim("char->integer", char_to_integer);
    register_prim("integer->char", integer_to_char);
    // symbols
    register_prim("symbol?", symbolp_proc);
    // strings
    register_prim("string?", stringp_proc);
    register_prim("string->symbol", string_to_symbol);
    register_prim("symbol->string", symbol_to_string);
    register_prim("string-append", string_append);
    // numbers
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
    // pairs and lists
    register_prim("null?", nullp_proc);
    register_prim("pair?", consp_proc);
    register_prim("cons", Mcons);
    register_prim("car", car_proc);
    register_prim("cdr", cdr_proc);
    register_prim("list?", listp_proc);
    register_prim("length", length_proc);
    register_prim("reverse", list_reverse);
    register_prim("append", list_append);
    // vectors
    register_prim("vector?", vectorp_proc);
    register_prim("vector-length", vector_length);
    register_prim("vector-ref", vector_ref);
    register_prim("vector-set!", vector_set);
    register_prim("list->vector", list_to_vector);
    // Procedure
    register_prim("procedure?", procedurep_proc);
    // I/O
    register_prim("write", write_proc);
    register_prim("newline", newline_proc);
    register_prim("flush-output", flush_proc);
    // environments
    register_prim("env_extend", env_extend);
    register_prim("env_set", env_set);
    register_prim("env_define", env_define);
    register_prim("env_get", env_get);
    register_prim("make_closure", Mclosure);
    // runtime utilities
    register_prim("error1", error1_proc);
    register_prim("error2", error2_proc);
    register_prim("arity_exn", arity_exn);
    register_prim("do_rest_arg", do_rest_arg);
}

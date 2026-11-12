/**
 * @file
 * The expander's support: gensym, aliases, `%strip`, and `%bound?`.
 */

#include "prims.h"

#include <inttypes.h>

#include "gc.h"
#include "table.h"

// Aliases are uninterned symbols whose value is `#(alias_tag id env)`.
static minim_obj alias_tag;

// The `#(alias_tag id env)` of the alias `x`, or `#f` if `x` isn't one.
static minim_obj alias_info(minim_obj x) {
    if (!minim_is_symbol(x))
        return MINIM_FALSE;
    minim_obj v = minim_symbol_value(x);
    return minim_is_vector(v) && minim_vector_length(v) == 3 && minim_vector_ref(v, 0) == alias_tag
        ? v : MINIM_FALSE;
}

static minim_obj prim_gensym(minim_obj *args, size_t n) {
    static uintptr_t count;
    if (n == 1) {
        check(minim_is_string(args[0]), "gensym: not a string", args[0]);
        return minim_make_uninterned(args[0]);
    }
    char name[32];
    snprintf(name, sizeof(name), "g%" PRIuPTR, count++);
    return minim_make_uninterned(minim_string_from_cstr(name));
}

static minim_obj prim_make_alias(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "%make-alias: not a symbol", args[0]);
    minim_obj alias = minim_make_uninterned(minim_symbol_name(args[0]));
    MINIM_FRAME(&alias);
    minim_obj info = minim_make_vector(3, alias_tag);
    minim_vector_set(info, 1, args[0]);
    minim_vector_set(info, 2, args[1]);
    minim_set_symbol_value(alias, info);
    MINIM_RETURN(alias);
}

static minim_obj prim_alias_original(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_obj info = alias_info(args[0]);
    return info == MINIM_FALSE ? info : minim_vector_ref(info, 1);
}

static minim_obj prim_alias_env(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_obj info = alias_info(args[0]);
    return info == MINIM_FALSE ? info : minim_vector_ref(info, 2);
}

// The original of the alias `x`, through chains of them, or `x` if it isn't one.
static minim_obj original(minim_obj x) {
    for (minim_obj info; (info = alias_info(x)) != MINIM_FALSE;)
        x = minim_vector_ref(info, 1);
    return x;
}

static size_t field_count(minim_obj x) {
    return minim_is_pair(x) ? 2 : minim_vector_length(x);
}

// Field `i` of the pair or vector `x`: a pair's car, then its cdr.
static minim_obj field(minim_obj x, size_t i) {
    if (minim_is_pair(x))
        return i == 0 ? minim_car(x) : minim_cdr(x);
    return minim_vector_ref(x, i);
}

static void set_field(minim_obj x, size_t i, minim_obj v) {
    if (!minim_is_pair(x))
        minim_vector_set(x, i, v);
    else if (i == 0)
        minim_set_car(x, v);
    else
        minim_set_cdr(x, v);
}

// Visits the pairs and vectors of `x`, each once, `x` first, in an order fixed
// by the data: stores each in the vector `into`, unless #f, and returns how
// many.  Sets `*alias` if one holds an alias.  Allocates nothing on the heap.
static size_t visit_data(minim_obj x, minim_obj into, bool *alias) {
    minim_walk w = {0};
    minim_walk_push(&w, x);
    size_t n = 0;
    while (w.len > 0) {
        x = w.items[--w.len];
        if (into != MINIM_FALSE)
            minim_vector_set(into, n, x);
        n++;
        for (size_t i = 0; i < field_count(x); i++) {
            minim_obj f = field(x, i);
            *alias = *alias || alias_info(f) != MINIM_FALSE;
            minim_walk_push(&w, f);
        }
    }
    minim_walk_free(&w);
    return n;
}

// `x` with every alias replaced by its original: `x` itself if it holds none,
// else a copy of its pairs and vectors, shaped as they are, cycles included.
// The table is keyed by address, so it is filled only once the copies are
// all allocated, with the originals held in a vector until then.
static minim_obj prim_strip(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    if (!minim_is_pair(args[0]) && !minim_is_vector(args[0]))
        return original(args[0]);
    bool alias = false;
    size_t count = visit_data(args[0], MINIM_FALSE, &alias);
    if (!alias)
        return args[0];

    minim_obj originals = minim_make_vector(count, MINIM_FALSE);
    minim_obj copies = MINIM_FALSE;
    MINIM_FRAME(&originals, &copies);
    visit_data(args[0], originals, &alias);
    copies = minim_make_vector(count, MINIM_FALSE);
    for (size_t i = 0; i < count; i++) {
        minim_obj o = minim_vector_ref(originals, i);
        minim_obj copy = minim_is_pair(o) ? minim_cons(MINIM_FALSE, MINIM_FALSE)
                                          : minim_make_vector(minim_vector_length(o), MINIM_FALSE);
        minim_vector_set(copies, i, copy);
    }

    minim_table index = {0};
    for (size_t i = 0; i < count; i++)
        *minim_table_add(&index, minim_vector_ref(originals, i)) = i;
    for (size_t i = 0; i < count; i++) {
        minim_obj o = minim_vector_ref(originals, i);
        minim_obj copy = minim_vector_ref(copies, i);
        for (size_t j = 0; j < field_count(o); j++) {
            minim_obj f = field(o, j);
            uintptr_t *k = minim_is_pair(f) || minim_is_vector(f) ? minim_table_find(&index, f)
                                                                  : NULL;
            set_field(copy, j, k != NULL ? minim_vector_ref(copies, *k) : original(f));
        }
    }
    minim_table_free(&index);
    MINIM_RETURN(minim_vector_ref(copies, 0));
}

static minim_obj prim_is_bound(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_symbol(args[0]), "%bound?: not a symbol", args[0]);
    return boolean(minim_symbol_value(args[0]) != MINIM_UNBOUND
                   && alias_info(args[0]) == MINIM_FALSE);
}

void minim_alias_init(void) {
    alias_tag = minim_make_uninterned(minim_string_from_cstr("alias"));
    minim_gc_add_global(&alias_tag);
}

const minim_prim_def minim_syntax_prims[] = {
    {"gensym", prim_gensym, 0, 1},
    {"%make-alias", prim_make_alias, 2, 2},
    {"%alias-original", prim_alias_original, 1, 1},
    {"%alias-env", prim_alias_env, 1, 1},
    {"%strip", prim_strip, 1, 1},
    {"%bound?", prim_is_bound, 1, 1},
    {NULL, NULL, 0, 0},
};

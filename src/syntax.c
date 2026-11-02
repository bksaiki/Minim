/**
 * @file
 * The expander's support: gensym, aliases, and `%bound?`.
 */

#include "prims.h"

#include <inttypes.h>

#include "gc.h"

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
    {"%bound?", prim_is_bound, 1, 1},
    {NULL, NULL, 0, 0},
};

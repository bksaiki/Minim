/**
 * @file
 * Defines the primitives, from every area's table, as global symbol values.
 */

#include "prims.h"

#include "common.h"

static const minim_prim_def *const tables[] = {
    minim_number_prims, minim_char_prims,   minim_pair_prims,   minim_control_prims,
    minim_equal_prims,  minim_vector_prims, minim_string_prims, minim_system_prims,
    minim_syntax_prims, minim_error_prims,  minim_record_prims,
};

static void define_primitive(const minim_prim_def *def) {
    minim_obj sym = minim_intern_cstr(def->name);
    MINIM_FRAME(&sym);
    minim_obj p = minim_make_primitive(sym, def->fn, def->min_arity, def->max_arity);
    minim_set_symbol_value(sym, p);
    MINIM_RETURN_VOID;
}

void minim_prims_init(void) {
    minim_alias_init();
    for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
        for (const minim_prim_def *def = tables[i]; def->name != NULL; def++)
            define_primitive(def);
    }
}

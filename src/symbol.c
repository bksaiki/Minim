/**
 * @file
 * Symbol interning.
 */

#include "symbol.h"

#include <string.h>

#include "heap.h"
#include "minim.h"

enum {
    INITIAL_CAPACITY = 64,  // power of 2
    EMPTY = MINIM_FALSE,
};

// Open-addressing table of symbols, hashed by name.  It is a vector in the
// GC heap, registered as a global root, so collection needs no special
// handling.
//
// Differences from Chez (c/intern.c): the table is strong, so unreferenced
// symbols are never collected; Chez prunes those without a value or property
// list.  Chez also chains buckets, caches each symbol's hash, and shrinks the
// table.
static minim_obj table;
static size_t count;

// Returns the slot holding the symbol named `str`, or the empty slot where it
// belongs.
static size_t find_slot(minim_obj str) {
    size_t mask = minim_vector_length(table) - 1;
    size_t i = minim_string_hash(str) & mask;
    for (;;) {
        minim_obj sym = minim_vector_ref(table, i);
        if (sym == EMPTY || minim_string_equal(minim_symbol_name(sym), str))
            return i;
        i = (i + 1) & mask;
    }
}

// Doubles the table.
static void grow(void) {
    minim_obj old = table;
    MINIM_FRAME(&old);
    table = minim_make_vector(2 * minim_vector_length(old), EMPTY);
    for (size_t i = 0; i < minim_vector_length(old); i++) {
        minim_obj sym = minim_vector_ref(old, i);
        if (sym != EMPTY)
            minim_vector_set(table, find_slot(minim_symbol_name(sym)), sym);
    }
    MINIM_RETURN_VOID;
}

minim_obj minim_sym_begin;
minim_obj minim_sym_define;
minim_obj minim_sym_define_values;
minim_obj minim_sym_expand;
minim_obj minim_sym_if;
minim_obj minim_sym_lambda;
minim_obj minim_sym_let_values;
minim_obj minim_sym_letrec_values;
minim_obj minim_sym_quasiquote;
minim_obj minim_sym_quote;
minim_obj minim_sym_raise_c_error;
minim_obj minim_sym_set;
minim_obj minim_sym_unquote;
minim_obj minim_sym_unquote_splicing;

// Interns `name` into the global `*var` and registers it as a root.
static void intern_global(minim_obj *var, const char *name) {
    *var = minim_intern_cstr(name);
    minim_gc_add_global(var);
}

void minim_symbol_init(void) {
    table = minim_make_vector(INITIAL_CAPACITY, EMPTY);
    count = 0;
    minim_gc_add_global(&table);

    intern_global(&minim_sym_begin, "begin");
    intern_global(&minim_sym_define, "define");
    intern_global(&minim_sym_define_values, "define-values");
    intern_global(&minim_sym_expand, "%expand");
    intern_global(&minim_sym_if, "if");
    intern_global(&minim_sym_lambda, "lambda");
    intern_global(&minim_sym_let_values, "let-values");
    intern_global(&minim_sym_letrec_values, "letrec-values");
    intern_global(&minim_sym_quasiquote, "quasiquote");
    intern_global(&minim_sym_quote, "quote");
    intern_global(&minim_sym_raise_c_error, "%raise-c-error");
    intern_global(&minim_sym_set, "set!");
    intern_global(&minim_sym_unquote, "unquote");
    intern_global(&minim_sym_unquote_splicing, "unquote-splicing");
}

minim_obj minim_intern(minim_obj str) {
    minim_obj found = minim_vector_ref(table, find_slot(str));
    if (found != EMPTY)
        return found;

    minim_obj sym = minim_make_uninterned(str);
    MINIM_FRAME(&sym);
    // Keep the load factor at most 1/2.
    if (2 * (count + 1) > minim_vector_length(table))
        grow();
    minim_vector_set(table, find_slot(minim_symbol_name(sym)), sym);
    count++;
    MINIM_RETURN(sym);
}

minim_obj minim_make_uninterned(minim_obj str) {
    minim_obj name = str;
    MINIM_FRAME(&str, &name);
    if (!(minim_typed_header(str) & MINIM_HEADER_IMMUTABLE)) {
        size_t n = minim_string_length(str);
        name = minim_make_string(n, 0);
        memcpy(minim_string_chars(name), minim_string_chars(str), n * sizeof(uint32_t));
        ((minim_obj *)(name - MINIM_TAG_TYPED))[0] |= MINIM_HEADER_IMMUTABLE;
    }

    minim_obj *p = minim_heap_alloc(MINIM_SYMBOL_SIZE);
    p[0] = name;
    p[1] = MINIM_UNBOUND;
    MINIM_RETURN(((minim_obj)p) | MINIM_TAG_SYMBOL);
}

minim_obj minim_intern_cstr(const char *s) {
    return minim_intern(minim_string_from_cstr(s));
}

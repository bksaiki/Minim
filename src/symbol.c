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

static uint64_t hash(minim_obj str) {
    const uint32_t *chars = minim_string_chars(str);
    size_t n = minim_string_length(str);
    uint64_t h = 14695981039346656037u;  // FNV-1a
    for (size_t i = 0; i < n; i++) {
        h ^= chars[i];
        h *= 1099511628211u;
    }
    return h;
}

static bool string_equal(minim_obj a, minim_obj b) {
    size_t n = minim_string_length(a);
    return n == minim_string_length(b)
        && memcmp(minim_string_chars(a), minim_string_chars(b), n * sizeof(uint32_t)) == 0;
}

// Returns the slot holding the symbol named `str`, or the empty slot where it
// belongs.
static size_t find_slot(minim_obj str) {
    size_t mask = minim_vector_length(table) - 1;
    size_t i = hash(str) & mask;
    for (;;) {
        minim_obj sym = minim_vector_ref(table, i);
        if (sym == EMPTY || string_equal(minim_symbol_name(sym), str))
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

void minim_symbol_init(void) {
    table = minim_make_vector(INITIAL_CAPACITY, EMPTY);
    count = 0;
    minim_gc_add_global(&table);
}

minim_obj minim_intern(minim_obj str) {
    minim_obj found = minim_vector_ref(table, find_slot(str));
    if (found != EMPTY)
        return found;

    minim_obj name = MINIM_NIL;
    MINIM_FRAME(&str, &name);
    size_t n = minim_string_length(str);
    name = minim_make_string(n, 0);
    memcpy(minim_string_chars(name), minim_string_chars(str), n * sizeof(uint32_t));
    ((minim_obj *)(name - MINIM_TAG_TYPED))[0] |= MINIM_HEADER_IMMUTABLE;

    minim_obj *p = minim_heap_alloc(MINIM_SYMBOL_SIZE);
    p[0] = name;
    p[1] = MINIM_UNBOUND;
    minim_obj sym = (minim_obj)p | MINIM_TAG_SYMBOL;

    // Keep the load factor at most 1/2.
    if (2 * (count + 1) > minim_vector_length(table)) {
        MINIM_PROTECT(&sym);
        grow();
    }
    minim_vector_set(table, find_slot(name), sym);
    count++;
    MINIM_RETURN(sym);
}

minim_obj minim_intern_cstr(const char *s) {
    return minim_intern(minim_string_from_cstr(s));
}

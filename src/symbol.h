/**
 * @file
 * Symbol table lifecycle and well-known symbols.  The symbol API is in
 * minim.h.
 */

#ifndef MINIM_SYMBOL_H
#define MINIM_SYMBOL_H

#include "minim.h"

/** The symbol `quote`. */
extern minim_obj minim_sym_quote;

/** Creates the symbol table and well-known symbols; call after minim_gc_init. */
void minim_symbol_init(void);

#endif

/**
 * @file
 * Symbol table lifecycle and well-known symbols.  The symbol API is in
 * minim.h.
 */

#ifndef MINIM_SYMBOL_H
#define MINIM_SYMBOL_H

#include "minim.h"

/** The symbol `begin`. */
extern minim_obj minim_sym_begin;

/** The symbol `define`. */
extern minim_obj minim_sym_define;

/** The symbol `define-values`. */
extern minim_obj minim_sym_define_values;

/** The symbol `%expand`, the expander called on each top-level form. */
extern minim_obj minim_sym_expand;

/** The symbol `%raise-c-error`, called on errors from C. */
extern minim_obj minim_sym_raise;

/** The symbol `if`. */
extern minim_obj minim_sym_if;

/** The symbol `lambda`. */
extern minim_obj minim_sym_lambda;

/** The symbol `let-values`. */
extern minim_obj minim_sym_let_values;

/** The symbol `letrec-values`. */
extern minim_obj minim_sym_letrec_values;

/** The symbol `quasiquote`. */
extern minim_obj minim_sym_quasiquote;

/** The symbol `quote`. */
extern minim_obj minim_sym_quote;

/** The symbol `set!`. */
extern minim_obj minim_sym_set;

/** The symbol `unquote`. */
extern minim_obj minim_sym_unquote;

/** The symbol `unquote-splicing`. */
extern minim_obj minim_sym_unquote_splicing;

/** Creates the symbol table and well-known symbols; call after minim_gc_init. */
void minim_symbol_init(void);

#endif

/**
 * @file
 * Environments.  `'()` is the global environment, which uses symbol value
 * slots; a local environment is a pair `(bindings . parent)` whose bindings
 * are an association list.
 */

#ifndef MINIM_ENV_H
#define MINIM_ENV_H

#include <stddef.h>

#include "minim.h"

/**
 * New environment binding `params` to the `n` values at `args` in `parent`.
 * `params` is a list, possibly improper with a rest symbol, whose arity
 * accepts `n`.  `args` may point into the value stack.
 */
minim_obj minim_env_extend(minim_obj params, minim_obj *args, size_t n, minim_obj parent);

/** Value of `sym` in `env`; raises if unbound. */
minim_obj minim_env_lookup(minim_obj env, minim_obj sym);

/**
 * Binds `sym` to `val`: globally if `env` is `'()`, otherwise in the innermost
 * frame of `env`, shadowing any binding of `sym` there.
 */
void minim_env_define(minim_obj env, minim_obj sym, minim_obj val);

/** Sets the existing binding of `sym` in `env`; raises if unbound. */
void minim_env_set(minim_obj env, minim_obj sym, minim_obj val);

#endif

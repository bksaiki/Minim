#ifndef _MINIM_ARITY_H_
#define _MINIM_ARITY_H_

#include "builtin.h"

// Sets 'parity' to the arity of fun
bool minim_get_builtin_arity(MinimBuiltin fun, MinimArity *parity);

// Checks the arity of a builtin function
bool minim_check_arity(MinimBuiltin fun, size_t argc, MinimEnv *env, MinimObject **perr);

#endif

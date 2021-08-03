#ifndef _MINIM_ARITY_H_
#define _MINIM_ARITY_H_

#include "builtin.h"

// Sets 'parity' to the arity of fun
bool minim_get_builtin_arity(MinimBuiltin fun, MinimArity *parity);

// Sets 'parity' to the arity of the syntax
bool minim_get_syntax_arity(MinimBuiltin fun, MinimArity *parity);

// Sets 'parity' to the arity of the lambda
bool minim_get_lambda_arity(MinimLambda *lam, MinimArity *parity);

// Checks the arity of a builtin function
bool minim_check_arity(MinimBuiltin fun, size_t argc, MinimEnv *env, MinimObject **perr);

// Checks the arity of syntax
bool minim_check_syntax_arity(MinimBuiltin fun, size_t argc, MinimEnv *env);

#endif

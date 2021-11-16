#ifndef _MINIM_LAMBDA_H_
#define _MINIM_LAMBDA_H_

#include "../common/path.h"
#include "env.h"
#include "syntax.h"

typedef struct MinimLambda
{
    MinimObject *body;
    SyntaxLoc *loc;
    MinimEnv *env;
    char **args, *rest, *name;
    int argc;
} MinimLambda;

// Functions

void init_minim_lambda(MinimLambda **plam);

// Evaluates the given lambda expression with `env` as the caller and
// lam->env as the previous namespace.
MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args);

// Like `eval_lambda` but transforms from the module of `env` are referencable.
MinimObject *eval_lambda2(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args);

void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf);

#endif

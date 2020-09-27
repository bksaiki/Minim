#ifndef _MINIM_LAMBDA_H_
#define _MINIM_LAMBDA_H_

#include "env.h"

typedef struct MinimLambda
{
    char *name;
    MinimObject *formals;
    MinimObject *body;
} MinimLambda;

// Functions

void init_minim_lambda(MinimLambda **plam);
void copy_minim_lambda(MinimLambda **cp, MinimLambda *src);
void free_minim_lambda(MinimLambda *lam);

// Evaluates the given lambda expression and stores the result at 'pobj'.
MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, int argc, MinimObject **args);

// Load builtins

void env_load_module_lambda(MinimEnv *env);

// Builtins

MinimObject *minim_builtin_lambda(MinimEnv *env, int argc, MinimObject **args);


#endif
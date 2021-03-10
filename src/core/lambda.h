#ifndef _MINIM_LAMBDA_H_
#define _MINIM_LAMBDA_H_

#include "../common/read.h"
#include "ast.h"
#include "env.h"

typedef struct MinimLambda
{
    MinimAst *body;
    SyntaxLoc *loc;
    MinimEnv *env;
    char **args, *rest, *name;
    int argc;
} MinimLambda;

// Functions

void init_minim_lambda(MinimLambda **plam);
void copy_minim_lambda(MinimLambda **cp, MinimLambda *src);
void free_minim_lambda(MinimLambda *lam);

// Evaluates the given lambda expression and stores the result at 'pobj'.
MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, MinimObject **args, size_t argc);

// Internals

bool assert_func(MinimObject *arg, MinimObject **ret, const char *msg);

bool minim_lambdap(MinimObject *thing);
bool minim_funcp(MinimObject *thing);

bool minim_lambda_equalp(MinimLambda *a, MinimLambda *b);
void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf);

#endif
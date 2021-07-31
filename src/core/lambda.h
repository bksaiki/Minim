#ifndef _MINIM_LAMBDA_H_
#define _MINIM_LAMBDA_H_

#include "../common/path.h"
#include "ast.h"
#include "env.h"

typedef struct MinimLambda
{
    SyntaxNode *body;
    SyntaxLoc *loc;
    MinimEnv *env;
    char **args, *rest, *name;
    int argc;
} MinimLambda;

// Functions

void init_minim_lambda(MinimLambda **plam);
void copy_minim_lambda(MinimLambda **cp, MinimLambda *src);

// Evaluates the given lambda expression and stores the result at 'pobj'.
MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args);

bool minim_lambda_equalp(MinimLambda *a, MinimLambda *b);
void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf);

#endif

#ifndef _MINIM_SYNTAX_H_
#define _MINIM_SYNTAX_H_

#include "ast.h"
#include "env.h"

// Throws an error if `ast` is ill-formatted syntax.
void check_syntax(MinimEnv *env, SyntaxNode *ast);

// Applies constant folding to `ast` and returns the result.
SyntaxNode *constant_fold(MinimEnv *env, SyntaxNode *ast);

// Converts an object to syntax.
SyntaxNode *datum_to_syntax(MinimEnv *env, MinimObject *obj);

#endif

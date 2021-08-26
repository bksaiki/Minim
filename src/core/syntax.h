#ifndef _MINIM_SYNTAX_H_
#define _MINIM_SYNTAX_H_

#include "ast.h"
#include "env.h"

void check_syntax(MinimEnv *env, SyntaxNode *ast);
SyntaxNode *datum_to_syntax(MinimEnv *env, MinimObject *obj);

#endif

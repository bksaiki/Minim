#ifndef _MINIM_SYNTAX_H_
#define _MINIM_SYNTAX_H_

#include "ast.h"
#include "env.h"

bool check_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr);

#endif

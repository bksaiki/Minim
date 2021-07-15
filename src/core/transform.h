#ifndef _MINIM_TRANSFORM_H_
#define _MINIM_TRANSFORM_H_

#include "ast.h"
#include "env.h"

bool transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr);

#endif

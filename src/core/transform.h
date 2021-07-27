#ifndef _MINIM_TRANSFORM_H_
#define _MINIM_TRANSFORM_H_

#include "ast.h"
#include "env.h"

// Applies syntax transforms on ast
SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr);

// Returns if a [match replace] transform is valid
bool valid_transformp(SyntaxNode *match, SyntaxNode *replace, MinimObject *reserved, MinimObject **perr);

#endif

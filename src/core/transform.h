#ifndef _MINIM_TRANSFORM_H_
#define _MINIM_TRANSFORM_H_

#include "ast.h"
#include "env.h"

// Applies syntax transforms on ast
SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast);

// Returns if a [match replace] transform is valid
void check_transform(SyntaxNode *match, SyntaxNode *replace, MinimObject *reserved);

#endif

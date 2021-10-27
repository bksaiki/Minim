#ifndef _MINIM_TRANSFORM_H_
#define _MINIM_TRANSFORM_H_

#include "ast.h"
#include "env.h"

// Applies syntax transforms on ast
MinimObject* transform_syntax(MinimEnv *env, MinimObject* ast);

// Returns if a [match replace] transform is valid
void check_transform(MinimEnv *env, MinimObject *match, MinimObject *replace, MinimObject *reserved);

#endif

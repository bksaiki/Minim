#ifndef _MINIM_EVAL_H_
#define _MINIM_EVAL_H_

#include "env.h"
#include "parser.h"

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'obj'. Returns a non-zero result on success.
int eval_ast(MinimEnv* env, MinimAstNode *ast, MinimObject **pobj);

#endif
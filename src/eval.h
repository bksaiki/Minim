#ifndef _MINIM_EVAL_H_
#define _MINIM_EVAL_H_

#include "env.h"
#include "parser.h"

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'pobj'. Returns a non-zero result on success.
int eval_ast(MinimEnv* env, MinimAstNode *ast, MinimObject **pobj);

// Evaluates the s-expression stored at 'expr' and stores the result
// at 'pobj'. Returns true on success.
int eval_sexpr(MinimEnv *env, MinimObject *expr, MinimObject **pobj);

#endif
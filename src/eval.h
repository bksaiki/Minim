#ifndef _MINIM_EVAL_H_
#define _MINIM_EVAL_H_

#include "object.h"
#include "parser.h"
#include "util.h"

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'obj'. Returns a non-zero result on success.
int eval_ast(MinimAstWrapper *ast, MinimObjectWrapper *objw);

#endif
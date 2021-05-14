#ifndef _MINIM_EVAL_H_
#define _MINIM_EVAL_H_

#include "env.h"
#include "lambda.h"
#include "parser.h"

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'pobj'. Returns a non-zero result on success.
int eval_ast(MinimEnv* env, SyntaxNode *ast, MinimObject **pobj);

// Same as 'eval_ast' except it does not run the syntax checker
int eval_ast_no_check(MinimEnv* env, SyntaxNode *ast, MinimObject **pobj);

// Evaluates the syntax node by one layer storing the result at 'pobj' and returning
// a non-zero result on success. If the syntax node is leaf, a single object is the result.
// Else a list of syntax children is the result.
int unsyntax_ast(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj);

// Recursively evaluates an ast.
int unsyntax_ast_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj);

// Evaluates an expression and returns a string. The result must be freed.
char *eval_string(char *str, size_t len);

#endif

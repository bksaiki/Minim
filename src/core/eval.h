#ifndef _MINIM_EVAL_H_
#define _MINIM_EVAL_H_

#include "env.h"
#include "lambda.h"
#include "module.h"
#include "parser.h"

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'pobj'. Returns a non-zero result on success.
MinimObject *eval_ast(MinimEnv* env, MinimObject *ast);

// Same as 'eval_ast' except it does not run the syntax checker
MinimObject *eval_ast_no_check(MinimEnv* env, MinimObject *ast);

// Evaluates a syntax terminal. Returns NULL on failure.
MinimObject *eval_ast_terminal(MinimEnv *env, MinimObject *ast);

// Evaluates `module` up to defining macros
void eval_module_cached(MinimModule *module);

// Evaluates `module` up to applying syntax macros
void eval_module_macros(MinimModule *module);

// Evaluates `module` and returns the result
MinimObject *eval_module(MinimModule *module);

// Evaluates the syntax node by one layer storing the result at 'pobj' and returning
// a non-zero result on success. If the syntax node is leaf, a single object is the result.
// Else a list of syntax children is the result.
MinimObject *unsyntax_ast(MinimEnv *env, MinimObject *ast);

// Recursively unwraps an ast.
MinimObject *unsyntax_ast_rec(MinimEnv *env, MinimObject *ast);

// Recursively unwraps an ast, full evaluation with `unquote`
MinimObject *unsyntax_ast_rec2(MinimEnv *env, MinimObject *ast);

// Evaluates an expression and returns a string.
char *eval_string(char *str, size_t len);

#endif

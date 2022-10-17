#ifndef _MINIM_COMPILER_H_
#define _MINIM_COMPILER_H_

#include "../core/minimpriv.h"

// Compiles a module.
void compile_module(MinimEnv *env, MinimModule *module);

// Compiles a single expression.
void compile_expr(MinimEnv *env, MinimObject *stx);

#endif

#ifndef _MINIM_COMPILER_JIT_H_
#define _MINIM_COMPILER_JIT_H_

#include "../core/minimpriv.h"
#include "compilepriv.h"

// Assemble a function into x86.
void function_assemble_x86(MinimEnv *env, Function *func);

#define ASSEMBLE    function_assemble_x86

#endif

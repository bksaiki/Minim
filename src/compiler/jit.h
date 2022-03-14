#ifndef _MINIM_COMPILER_JIT_H_
#define _MINIM_COMPILER_JIT_H_

#include "../core/minimpriv.h"
#include "compilepriv.h"

// Assemble a function into x86.
void function_assemble_x86(MinimEnv *env, Function *func, Buffer *bf);

// Resolve address
uintptr_t resolve_address(MinimObject *stx);

#define ASSEMBLE(e, f, b)    function_assemble_x86(e, f, b)

#endif

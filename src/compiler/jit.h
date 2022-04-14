#ifndef _MINIM_COMPILER_JIT_H_
#define _MINIM_COMPILER_JIT_H_

#include "../core/minimpriv.h"
#include "compilepriv.h"

// JIT wrapper for `env_get_sym`.
MinimObject *jit_get_sym(MinimEnv *env, MinimObject *sym);

// JIT wrapper for `env_intern_sym`.
#define jit_set_sym     env_intern_sym

// Assemble a function into x86.
void function_assemble_x86(MinimEnv *env, Function *func, Buffer *bf);

// Resolve address of an builtin procedure.
uintptr_t resolve_address(MinimEnv *env, MinimObject *stx);

#define ASSEMBLE(e, f, b)    function_assemble_x86(e, f, b)

#endif

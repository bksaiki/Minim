#ifndef _MINIMC_COMPILER_H_
#define _MINIMC_COMPILER_H_

#include "../core/module.h"

// Compiles a module and writes the instructions to a buffer
Buffer *compile_module(MinimEnv *env, MinimModule *module);

#endif

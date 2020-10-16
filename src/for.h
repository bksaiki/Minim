#ifndef _MINIM_FOR_H_
#define _MINIM_FOR_H_

#include "env.h"

// Builtins

void env_load_module_for(MinimEnv *env);

MinimObject *minim_builtin_for(MinimEnv *env, int argc, MinimObject **args);

#endif
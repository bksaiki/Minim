#ifndef _MINIM_BOOL_H_
#define _MINIM_BOOL_H_

#include "env.h"

void env_load_module_bool(MinimEnv *env);


// Builtins for use in the language
MinimObject *minim_builtin_not(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_or(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_and(MinimEnv *env, int argc, MinimObject** args);

#endif
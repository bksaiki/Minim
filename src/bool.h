#ifndef _MINIM_BOOL_H_
#define _MINIM_BOOL_H_

#include "env.h"

// Internals
bool coerce_into_bool(MinimObject *obj);
bool minim_boolp(MinimObject *thing);

// Builtins for use in the language

void env_load_module_bool(MinimEnv *env);

MinimObject *minim_builtin_boolp(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_not(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_or(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_and(MinimEnv *env, int argc, MinimObject** args);

#endif
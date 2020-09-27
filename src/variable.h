#ifndef _MINIM_VARIABLE_H_
#define _MINIM_VARIABLE_H_

#include "env.h"

void env_load_module_variable(MinimEnv *env);

MinimObject *minim_builtin_def(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_let(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_letstar(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_quote(MinimEnv *env, int argc, MinimObject **args);

// TODO: move these
MinimObject *minim_builtin_numberp(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_symbolp(MinimEnv *env, int argc, MinimObject **args);

#endif
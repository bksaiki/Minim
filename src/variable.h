#ifndef _MINIM_VARIABLE_H_
#define _MINIM_VARIABLE_H_

#include "env.h"

bool assert_number(MinimObject *obj, MinimObject **res, const char *msg);

// Internals

bool minim_numberp(MinimObject *obj);
bool minim_symbolp(MinimObject *obj);

// Builtins

void env_load_module_variable(MinimEnv *env);

MinimObject *minim_builtin_if(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_def(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_let(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_letstar(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_quote(MinimEnv *env, int argc, MinimObject **args);

// TODO: move these
MinimObject *minim_builtin_numberp(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_symbolp(MinimEnv *env, int argc, MinimObject **args);

#endif
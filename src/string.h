#ifndef _MINIM_STRING_H_
#define _MINIM_STRING_H_

#include "env.h"

// Internals

bool minim_stringp(MinimObject *thing);
bool assert_string(MinimObject *thing, MinimObject **res, const char *msg);

// Builtins

void env_load_module_string(MinimEnv *env);

MinimObject *minim_builtin_stringp(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_string_append(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_substring(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_string_to_symbol(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_symbol_to_string(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_format(MinimEnv *env, int argc, MinimObject **args);

MinimObject *minim_builtin_printf(MinimEnv *env, int argc, MinimObject **args);

#endif
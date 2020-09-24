#ifndef _MINIM_MATH_H_
#define _MINIM_MATH_H_

#include "env.h"
#include "object.h"

// Top-level mathematical functions

MinimObject *minim_builtin_add(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_sub(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_mul(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_div(MinimEnv *env, int argc, MinimObject** args);

#endif
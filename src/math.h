#ifndef _MINIM_MATH_H_
#define _MINIM_MATH_H_

#include "object.h"

// Top-level mathematical functions

MinimObject *minim_builtin_add(int argc, MinimObject** args);
MinimObject *minim_builtin_sub(int argc, MinimObject** args);
MinimObject *minim_builtin_mul(int argc, MinimObject** args);
MinimObject *minim_builtin_div(int argc, MinimObject** args);

#endif
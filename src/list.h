#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "object.h"

MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args);

#endif
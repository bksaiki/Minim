#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "object.h"

#define MINIM_CAR(x)   (((MinimObject**) x->data)[0])
#define MINIM_CDR(x)   (((MinimObject**) x->data)[1])

MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args);

MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_first(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_last(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_length(MinimEnv *env, int argc, MinimObject** args);

#endif
#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "env.h"

#define MINIM_CAR(x)   (((MinimObject**) x->data)[0])
#define MINIM_CDR(x)   (((MinimObject**) x->data)[1])

void env_load_list(MinimEnv *env);

int minim_list_length(MinimObject *list);

MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args);

MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_head(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_tail(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_length(MinimEnv *env, int argc, MinimObject** args);

#endif
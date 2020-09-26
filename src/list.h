#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "env.h"

#define MINIM_CAR(x)   (((MinimObject**) x->data)[0])
#define MINIM_CDR(x)   (((MinimObject**) x->data)[1])

#define MINIM_CAAR(x)   MINIM_CAR(MINIM_CAR(x))
#define MINIM_CDAR(x)   MINIM_CDR(MINIM_CAR(x))
#define MINIM_CADR(x)   MINIM_CAR(MINIM_CDR(x))
#define MINIM_CDDR(x)   MINIM_CDR(MINIM_CDR(x))

void env_load_module_list(MinimEnv *env);

// Assertions
bool assert_list_len(MinimObject *arg, MinimObject **ret, const char *op, int len);

// Internal versions of the builtins
bool minim_consp(MinimObject* thing);
bool minim_listp(MinimObject* thing);
int minim_list_length(MinimObject *list);

// Builtin functions in the language
MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_consp(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args);

MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_listp(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_head(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_tail(MinimEnv *env, int argc, MinimObject** args);
MinimObject *minim_builtin_length(MinimEnv *env, int argc, MinimObject** args);

#endif
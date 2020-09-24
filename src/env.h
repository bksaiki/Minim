#ifndef _MINIM_ENV_
#define _MINIM_ENV_

#include "object.h"

typedef struct MinimEnv
{
    int count;
    char **syms;
    MinimObject **vals;
} MinimEnv;

// Returns a new environment object.
void init_env(MinimEnv **penv);

// Returns a copy of the object associated with the symbol. Returns NULL if
// the symbol is not in the table. The returned value must be freed.
MinimObject *env_get_sym(MinimEnv *env, const char *sym);

// Adds 'sym' and 'obj' to the variable table.
void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj);

// Returns a pointer to the object associated with the symbol. Returns NULL
// if the symbol is not in the table.
MinimObject *env_peek_sym(MinimEnv *env, const char* sym);

// Deletes the given environment object
void free_env(MinimEnv *env);

#endif
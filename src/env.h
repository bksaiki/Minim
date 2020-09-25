#ifndef _MINIM_ENV_
#define _MINIM_ENV_

#include "object.h"

typedef struct MinimEnv
{
    int count;
    char **syms;
    MinimObject **vals;
} MinimEnv;

// Initializes a new environment object.
void init_env(MinimEnv **penv);

// Returns a copy of the object associated with the symbol. Returns NULL if
// the symbol is not in the table. The returned value must be freed.
MinimObject *env_get_sym(MinimEnv *env, const char *sym);

// Adds 'sym' and 'obj' to the variable table.
void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj);

// Returns a pointer to the key associated with the values. Returns NULL
// if the value is not in the table
const char *env_peek_key(MinimEnv *env, MinimObject *value);

// Returns a pointer to the object associated with the symbol. Returns NULL
// if the symbol is not in the table.
MinimObject *env_peek_sym(MinimEnv *env, const char* sym);

// Deletes the given environment object
void free_env(MinimEnv *env);

// Loads a single function into the environment
void env_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...);

// Loads every builtin symbol in the base library.
void env_load_builtins(MinimEnv *env);

#endif
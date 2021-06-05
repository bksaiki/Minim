#ifndef _MINIM_ENV_
#define _MINIM_ENV_

#include "object.h"
#include "print.h" // debugging
#include "symbols.h"

typedef struct MinimEnv
{
    struct MinimEnv *parent;
    MinimSymbolTable *table;
    size_t sym_count;
    bool copied;
} MinimEnv;

// Initializes a new environment object.
void init_env(MinimEnv **penv, MinimEnv *parent);

// Recursively copies the stack of environment objects
// Ignores the lowest environment
void rcopy_env(MinimEnv **penv, MinimEnv *src);

// Returns a copy of the object associated with the symbol. Returns NULL if
// the symbol is not in the table
MinimObject *env_get_sym(MinimEnv *env, const char *sym);

// Adds 'sym' and 'obj' to the variable table.
void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj);

// Sets 'sym' to 'obj'. Returns zero if 'sym' is not found.
int env_set_sym(MinimEnv *env, const char* sym, MinimObject *obj);

// Returns a pointer to the key associated with the values. Returns NULL
// if the value is not in the table
const char *env_peek_key(MinimEnv *env, MinimObject *value);

// Returns a pointer to the object associated with the symbol. Returns NULL
// if the symbol is not in the table.
MinimObject *env_peek_sym(MinimEnv *env, const char* sym);

// Returns the number of symbols in the environment
size_t env_symbol_count(MinimEnv *env);

#endif

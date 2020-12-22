#ifndef _MINIM_HASH_H_
#define _MINIM_HASH_H_

#include "env.h"

#define MINIM_DEFAULT_HASH_TABLE_SIZE       10
#define MINIM_DEFAULT_LOAD_FACTOR           0.75

struct MinimHashTableRow
{
    MinimObject **arr;
    size_t size;
} typedef MinimHashTableRow;

struct MinimHashTable
{
    MinimHashTableRow *arr;
    size_t len;
} typedef MinimHashTable;

void init_minim_hash_table(MinimHashTable **pht);
void copy_minim_hash_table(MinimHashTable **pht, MinimHashTable *src);
void free_minim_hash_table(MinimHashTable *ht);

void minim_hash_table_add(MinimHashTable *ht, MinimObject *k, MinimObject *v);

// Internals

bool assert_hash(MinimObject *arg, MinimObject **ret, const char *msg);

bool minim_hashp(MinimObject *thing);

// Builtins

void env_load_module_hash(MinimEnv *env);

MinimObject *minim_builtin_hash(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_hash_set(MinimEnv *env, int argc, MinimObject **args);

#endif
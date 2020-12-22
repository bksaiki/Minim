#ifndef _MINIM_HASH_H_
#define _MINIM_HASH_H_

#include "env.h"

#define MINIM_DEFAULT_HASH_TABLE_SIZE       10
#define MINIM_DEFAULT_HASH_TABLE_FACTOR     0.75

struct MinimHashRow
{
    MinimObject **arr;
    size_t len;
} typedef MinimHashRow;

struct MinimHash
{
    MinimHashRow *arr;
    size_t size;
    size_t elems;
} typedef MinimHash;

void init_minim_hash_table(MinimHash **pht);
void copy_minim_hash_table(MinimHash **pht, MinimHash *src);
void free_minim_hash_table(MinimHash *ht);

void minim_hash_table_add(MinimHash *ht, MinimObject *k, MinimObject *v);
bool minim_hash_table_keyp(MinimHash *ht, MinimObject *k);
MinimObject *minim_hash_table_ref(MinimHash *ht, MinimObject *k);
void minim_hash_table_remove(MinimHash *ht, MinimObject *k);

// Internals

bool assert_hash(MinimObject *arg, MinimObject **ret, const char *msg);

bool minim_hashp(MinimObject *thing);

// Builtins

void env_load_module_hash(MinimEnv *env);

MinimObject *minim_builtin_hash(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_hash_keyp(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_hash_ref(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_hash_remove(MinimEnv *env, int argc, MinimObject **args);
MinimObject *minim_builtin_hash_set(MinimEnv *env, int argc, MinimObject **args);

#endif
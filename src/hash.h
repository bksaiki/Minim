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

void free_minim_hash_table(MinimHashTable *ht);

void minim_hash_table_add(MinimHashTable *ht, MinimObject *k, MinimObject *v);

#endif
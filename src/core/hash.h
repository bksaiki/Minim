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
bool minim_hash_table_eqp(MinimHash *a, MinimHash *b);

uint32_t hash_bytes(const void* data, size_t len);

#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "common/buffer.h"
#include "assert.h"
#include "hash.h"
#include "list.h"

//
/// Hashing (Jenkins hash function)
//

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

static int64_t hash_bytes(void *data, size_t length, uint64_t seed)
{
    uint32_t a, b, c, offset;
    uint32_t *k = (uint32_t*) data;

    a = b = c = 0xdeadbeef + length + (seed >> 32);
    c += (seed & 0xffffffff);

    while (length > 12)
    {
        a += k[offset + 0];
        a += k[offset + 1] << 8;
        a += k[offset + 2] << 16;
        a += k[offset + 3] << 24;
        b += k[offset + 4];
        b += k[offset + 5] << 8;
        b += k[offset + 6] << 16;
        b += k[offset + 7] << 24;
        c += k[offset + 8];
        c += k[offset + 9] << 8;
        c += k[offset + 10] << 16;
        c += k[offset + 11] << 24;

        final(a, b, c);
        length -= 12;
        offset += 12;
    }

    switch (length) {
        case 12:
            c += k[offset + 11] << 24;
        case 11:
            c += k[offset + 10] << 16;
        case 10:
            c += k[offset + 9] << 8;
        case 9:
            c += k[offset + 8];
        case 8:
            b += k[offset + 7] << 24;
        case 7:
            b += k[offset + 6] << 16;
        case 6:
            b += k[offset + 5] << 8;
        case 5:
            b += k[offset + 4];
        case 4:
            a += k[offset + 3] << 24;
        case 3:
            a += k[offset + 2] << 16;
        case 2:
            a += k[offset + 1] << 8;
        case 1:
            a += k[offset + 0];
            break;
        case 0:
            return ((((int64_t) c) << 32) | ((int64_t) (b & 0xFFFFFFFFL)));
    }

    final(a, b, c);
    return ((((int64_t) c) << 32) | ((int64_t) (b & 0xFFFFFFFFL)));
}

//
//  Hash table
//

void init_minim_hash_table(MinimHash **pht)
{
    MinimHash *ht = malloc(sizeof(MinimHash));
    ht->arr = malloc(MINIM_DEFAULT_HASH_TABLE_SIZE * sizeof(MinimHashRow));
    ht->size = MINIM_DEFAULT_HASH_TABLE_SIZE;
    ht->elems = 0;
    *pht = ht;
    
    for (size_t i = 0; i < ht->size; ++i)
    {
        ht->arr[i].arr = NULL;
        ht->arr[i].len = 0;
    }
}

void copy_minim_hash_table(MinimHash **pht, MinimHash *src)
{
    MinimHash *ht = malloc(sizeof(MinimHash));
    ht->arr = malloc(src->size * sizeof(MinimHashRow));
    ht->size = src->size;
    ht->elems = src->elems;
    *pht = ht;

    for (size_t i = 0; i < ht->size; ++i)
    {
        ht->arr[i].arr = malloc(src->arr[i].len * sizeof(MinimObject*));
        ht->arr[i].len = src->arr[i].len;

        for (size_t j = 0; j < src->arr[i].len; ++j)
            copy_minim_object(&ht->arr[i].arr[j], src->arr[i].arr[j]);
    }
}

void free_minim_hash_table(MinimHash *ht)
{
    for (size_t i = 0; i < ht->size; ++i)
        free_minim_objects(ht->arr[i].len, ht->arr[i].arr);

    free(ht->arr);
    free(ht);
}

static void rehash_table(MinimHash *ht)
{
    MinimHashRow *htr;
    Buffer *bf;
    int64_t hash, reduc;
    size_t newSize = 2 * ht->size;
    
    htr = malloc(newSize * sizeof(MinimHashRow));
    for (size_t i = 0; i < newSize; ++i)
    {
        htr[i].arr = NULL;
        htr[i].len = 0;
    }

    for (size_t i = 0; i < ht->size; ++i)
    {
        for (size_t j = 0; j < ht->arr[i].len; ++j)
        {
            bf = minim_obj_to_bytes(MINIM_CAR(ht->arr[i].arr[j]));
            hash = hash_bytes(bf->data, bf->pos, 0);
            reduc = hash % newSize;

            if (htr[reduc].len == 0)
            {
                htr[reduc].arr = realloc(htr[reduc].arr, sizeof(MinimObject*));
                htr[reduc].arr[0] = ht->arr[i].arr[j];
                htr[reduc].len = 1;
            }
            else
            {      
                htr[reduc].arr = realloc(htr[reduc].arr, (htr[reduc].len + 1) * sizeof(MinimObject*));
                htr[reduc].arr[htr[reduc].len] = ht->arr[i].arr[j];
                ++htr[reduc].len;
            }
        }

        free(ht->arr[i].arr);
    }

    free(ht->arr);
    ht->arr = htr;
    ht->size = newSize;
    
}

void minim_hash_table_add(MinimHash *ht, MinimObject *k, MinimObject *v)
{
    MinimObject *ck, *cv;

    Buffer *bf = minim_obj_to_bytes(k);
    int64_t hash = hash_bytes(bf->data, bf->pos, 0);
    int64_t reduc = hash % ht->size;

    ++ht->elems;
    copy_minim_object(&ck, k);
    copy_minim_object(&cv, v);

    if (((double)ht->elems / (double)ht->size) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        rehash_table(ht); // rehash if too deep

    if (ht->arr[reduc].len == 0)
    {
        ht->arr[reduc].arr = realloc(ht->arr[reduc].arr, sizeof(MinimObject*));
        ht->arr[reduc].len = 1;
        init_minim_object(&ht->arr[reduc].arr[0], MINIM_OBJ_PAIR, ck, cv);
    }
    else
    {      
        ht->arr[reduc].arr = realloc(ht->arr[reduc].arr, (ht->arr[reduc].len + 1) * sizeof(MinimObject*));
        init_minim_object(&ht->arr[reduc].arr[ht->arr[reduc].len], MINIM_OBJ_PAIR, ck, cv);
        ++ht->arr[reduc].len;
    }
}

//
//  Internals
//

bool assert_hash(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_hashp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool minim_hashp(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_HASH;
}

//
//  Builtins
//

void env_load_module_hash(MinimEnv *env)
{
    env_load_builtin(env, "hash", MINIM_OBJ_FUNC, minim_builtin_hash);
    env_load_builtin(env, "hash-set", MINIM_OBJ_FUNC, minim_builtin_hash_set);
}

MinimObject *minim_builtin_hash(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "Expected 0 arguments for 'hash'", 0))
    {   
        MinimHash *ht;
        init_minim_hash_table(&ht);
        init_minim_object(&res, MINIM_OBJ_HASH, ht);
    }

    return res;
}

MinimObject *minim_builtin_hash_set(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "Expected 3 arguments for 'hash-set'", 3) &&
        assert_hash(args[0], &res, "Expected a hash table in the first argument of 'hash-set'"))
    {
        copy_minim_object(&res, args[0]);
        minim_hash_table_add(res->data, args[1], args[2]);
    }

    return res;
}
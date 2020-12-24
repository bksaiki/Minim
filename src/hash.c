#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "common/buffer.h"
#include "assert.h"
#include "hash.h"
#include "list.h"

//
//  Hash function
//
//  Adapted from burtleburle.net/bob/hash/doobs.html
//

#define hashsize(n) ((uint32_t)1 << (n))
#define hashmask(n) (hashsize(n) - 1)

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

static uint32_t hash_bytes(void* data, size_t length, uint32_t seed)
{
    uint8_t* k;
    uint32_t a, b, c, len;

    /* Set up the internal state */
    k = data;
    len = length;
    a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
    c = seed;         /* the previous hash value */

    /*---------------------------------------- handle most of the key */
    while (len >= 12)
    {
        a += (k[0] +((uint32_t)k[1] << 8) +((uint32_t)k[2] << 16) +((uint32_t)k[3] << 24));
        b += (k[4] +((uint32_t)k[5] << 8) +((uint32_t)k[6] << 16) +((uint32_t)k[7] << 24));
        c += (k[8] +((uint32_t)k[9] << 8) +((uint32_t)k[10] << 16)+((uint32_t)k[11] << 24));
        mix(a,b,c);
        k += 12; len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c += length;
    switch(len)              /* all the case statements fall through */
    {
    case 11: c += ((uint32_t)k[10] << 24);
    case 10: c += ((uint32_t)k[9] << 16);
    case 9 : c += ((uint32_t)k[8] << 8);
        /* the first byte of c is reserved for the length */
    case 8 : b += ((uint32_t)k[7] << 24);
    case 7 : b += ((uint32_t)k[6] << 16);
    case 6 : b += ((uint32_t)k[5] << 8);
    case 5 : b += k[4];
    case 4 : a += ((uint32_t)k[3] << 24);
    case 3 : a += ((uint32_t)k[2] << 16);
    case 2 : a += ((uint32_t)k[1] << 8);
    case 1 : a += k[0];
        /* case 0: nothing left to add */
    }
    mix(a,b,c);
    /*-------------------------------------------- report the result */
    return c;
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
        free_minim_objects(ht->arr[i].arr, ht->arr[i].len);

    free(ht->arr);
    free(ht);
}

static void rehash_table(MinimHash *ht)
{
    MinimHashRow *htr;
    Buffer *bf;
    uint32_t hash, reduc;
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
            hash = hash_bytes(bf->data, bf->pos, (uint32_t)0x12345678);
            reduc = hash % newSize;

            free_buffer(bf);
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
    uint32_t hash = hash_bytes(bf->data, bf->pos, (uint32_t)0x12345678);
    uint32_t reduc = hash % ht->size;

    ++ht->elems;
    free_buffer(bf);

    if (((double)ht->elems / (double)ht->size) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        rehash_table(ht); // rehash if too deep

    if (ht->arr[reduc].len == 0)
    {
        copy_minim_object(&ck, k);
        copy_minim_object(&cv, v);

        ht->arr[reduc].arr = realloc(ht->arr[reduc].arr, sizeof(MinimObject*));
        ht->arr[reduc].len = 1;
        init_minim_object(&ht->arr[reduc].arr[0], MINIM_OBJ_PAIR, ck, cv);
    }
    else
    {      
        copy_minim_object(&cv, v);

        // Check if key already exists
        for (size_t i = 0; i < ht->arr[reduc].len; ++i)
        {
            if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            {
                free_minim_object(MINIM_CDR(ht->arr[reduc].arr[i]));
                MINIM_CDR(ht->arr[reduc].arr[i]) = cv;
                return;
            }
        }

        copy_minim_object(&ck, k);
        ht->arr[reduc].arr = realloc(ht->arr[reduc].arr, (ht->arr[reduc].len + 1) * sizeof(MinimObject*));
        init_minim_object(&ht->arr[reduc].arr[ht->arr[reduc].len], MINIM_OBJ_PAIR, ck, cv);
        ++ht->arr[reduc].len;
    }
}

bool minim_hash_table_keyp(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, (uint32_t)0x12345678);
    uint32_t reduc = hash % ht->size;

    free_buffer(bf);
    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            return true;
    }

    return false;
}

MinimObject *minim_hash_table_ref(MinimHash *ht, MinimObject *k)
{
    MinimObject *cp = NULL;
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, (uint32_t)0x12345678);
    uint32_t reduc = hash % ht->size;

    free_buffer(bf);
    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            copy_minim_object(&cp, MINIM_CDR(ht->arr[reduc].arr[i]));
    }

    return cp;
}

void minim_hash_table_remove(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, (uint32_t)0x12345678);
    uint32_t reduc = hash % ht->size;

    free_buffer(bf);
    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
        {
            free_minim_object(ht->arr[reduc].arr[i]);
            ht->arr[reduc].arr[i] = ht->arr[reduc].arr[ht->arr[reduc].len - 1];
            --ht->arr[reduc].len;
            --ht->elems;
            ht->arr[reduc].arr = realloc(ht->arr[reduc].arr, ht->arr[reduc].len * sizeof(MinimObject*));
        }
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
    env_load_builtin(env, "hash-key?", MINIM_OBJ_FUNC, minim_builtin_hash_keyp);
    env_load_builtin(env, "hash-ref", MINIM_OBJ_FUNC, minim_builtin_hash_ref);
    env_load_builtin(env, "hash-remove", MINIM_OBJ_FUNC, minim_builtin_hash_remove);
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

MinimObject *minim_builtin_hash_keyp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "Expected 2 arguments for 'hash-key?'", 2) &&
        assert_hash(args[0], &res, "Expected a hash table in the first argument of 'hash-key?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL,
                          minim_hash_table_keyp(args[0]->data, args[1]));
    }

    return res;
}

MinimObject *minim_builtin_hash_ref(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "Expected 2 arguments for 'hash-ref'", 2) &&
        assert_hash(args[0], &res, "Expected a hash table in the first argument of 'hash-ref'"))
    {
        res = minim_hash_table_ref(args[0]->data, args[1]);
        if (!res)
        {
            Buffer *bf;
            PrintParams pp;

            init_buffer(&bf);
            writes_buffer(bf, "hash-ref: no value found for ");
            print_to_buffer(bf, args[1], env, &pp);
            minim_error(&res, bf->data);
            free_buffer(bf);
        }
    }

    return res;
}

MinimObject *minim_builtin_hash_remove(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "Expected 2 arguments for 'hash-remove'", 2) &&
        assert_hash(args[0], &res, "Expected a hash table in the first argument of 'hash-ref'"))
    {
        copy_minim_object(&res, args[0]);
        if (minim_hash_table_keyp(args[0]->data, args[1]))
            minim_hash_table_remove(res->data, args[1]);
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
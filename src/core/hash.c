#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "../gc/gc.h"
#include "../common/buffer.h"
#include "assert.h"
#include "error.h"
#include "hash.h"
#include "list.h"

//
//  Hash function
//
//  Adapted from burtleburle.net/bob/hash/doobs.html
//

#define hashsize(n) ((uint32_t)1 << (n))
#define hashmask(n) (hashsize(n) - 1)

#define hashseed   ((uint32_t)  0xdeadbeef)

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

uint32_t hash_bytes(const void* data, size_t length, uint32_t seed)
{
    uint8_t* k;
    uint32_t a, b, c, len;

    /* Set up the internal state */
    k = (uint8_t*)data;
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

static void gc_minim_hash_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((MinimHash*) ptr)->arr);
}

//
//  Hash table
//

void init_minim_hash_table(MinimHash **pht)
{
    MinimHash *ht = GC_alloc_opt(sizeof(MinimHash), NULL, gc_minim_hash_mrk);
    ht->arr = GC_alloc(MINIM_DEFAULT_HASH_TABLE_SIZE * sizeof(MinimHashRow));
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
    MinimHash *ht = GC_alloc_opt(sizeof(MinimHash), NULL, gc_minim_hash_mrk);
    ht->arr = GC_alloc(src->size * sizeof(MinimHashRow));
    ht->size = src->size;
    ht->elems = src->elems;
    *pht = ht;

    for (size_t i = 0; i < ht->size; ++i)
    {
        ht->arr[i].arr = GC_alloc(src->arr[i].len * sizeof(MinimObject*));
        ht->arr[i].len = src->arr[i].len;

        for (size_t j = 0; j < src->arr[i].len; ++j)
            copy_minim_object(&ht->arr[i].arr[j], src->arr[i].arr[j]);
    }
}

static void rehash_table(MinimHash *ht)
{
    MinimHashRow *htr;
    Buffer *bf;
    uint32_t hash, reduc;
    size_t newSize = 2 * ht->size;
    
    htr = GC_alloc(newSize * sizeof(MinimHashRow));
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
            hash = hash_bytes(bf->data, bf->pos, hashseed);
            reduc = hash % newSize;

            if (htr[reduc].len == 0)
            {
                htr[reduc].arr = GC_realloc(htr[reduc].arr, sizeof(MinimObject*));
                htr[reduc].arr[0] = ht->arr[i].arr[j];
                htr[reduc].len = 1;
            }
            else
            {      
                htr[reduc].arr = GC_realloc(htr[reduc].arr, (htr[reduc].len + 1) * sizeof(MinimObject*));
                htr[reduc].arr[htr[reduc].len] = ht->arr[i].arr[j];
                ++htr[reduc].len;
            }
        }
    }

    ht->arr = htr;
    ht->size = newSize;
    
}

static void minim_hash_table_add(MinimHash *ht, MinimObject *k, MinimObject *v)
{
    MinimObject *ck, *cv;

    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, hashseed);
    uint32_t reduc = hash % ht->size;

    ++ht->elems;
    if (((double)ht->elems / (double)ht->size) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        rehash_table(ht); // rehash if too deep

    if (ht->arr[reduc].len == 0)
    {
        copy_minim_object(&ck, k);
        copy_minim_object(&cv, v);

        ht->arr[reduc].arr = GC_realloc(ht->arr[reduc].arr, sizeof(MinimObject*));
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
                MINIM_CDR(ht->arr[reduc].arr[i]) = cv;
                return;
            }
        }

        copy_minim_object(&ck, k);
        ht->arr[reduc].arr = GC_realloc(ht->arr[reduc].arr, (ht->arr[reduc].len + 1) * sizeof(MinimObject*));
        init_minim_object(&ht->arr[reduc].arr[ht->arr[reduc].len], MINIM_OBJ_PAIR, ck, cv);
        ++ht->arr[reduc].len;
    }
}

static bool minim_hash_table_keyp(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, hashseed);
    uint32_t reduc = hash % ht->size;

    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            return true;
    }

    return false;
}

static MinimObject *minim_hash_table_ref(MinimHash *ht, MinimObject *k)
{
    MinimObject *cp = NULL;
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, hashseed);
    uint32_t reduc = hash % ht->size;

    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            copy_minim_object(&cp, MINIM_CDR(ht->arr[reduc].arr[i]));
    }

    return cp;
}

static void minim_hash_table_remove(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos, hashseed);
    uint32_t reduc = hash % ht->size;

    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
        {
            ht->arr[reduc].arr[i] = ht->arr[reduc].arr[ht->arr[reduc].len - 1];
            --ht->arr[reduc].len;
            --ht->elems;
            ht->arr[reduc].arr = GC_realloc(ht->arr[reduc].arr, ht->arr[reduc].len * sizeof(MinimObject*));
        }
    }
}

static MinimObject *minim_hash_table_to_list(MinimHash *ht)
{
    MinimObject *head, *it, *cp;
    bool first = true;

    for (size_t i = 0; i < ht->size; ++i)
    {
        for (size_t j = 0; j < ht->arr[i].len; ++j)
        {
            copy_minim_object(&cp, ht->arr[i].arr[j]);
            if (first)
            {
                init_minim_object(&head, MINIM_OBJ_PAIR, cp, NULL);
                it = head;
                first = false;
            }
            else
            {
                init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, cp, NULL);
                it = MINIM_CDR(it);
            }
        }
    }

    if (first)
        init_minim_object(&head, MINIM_OBJ_PAIR, NULL, NULL);

    return head;
}

//
//  Builtins
//

MinimObject *minim_builtin_hash(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimHash *ht;
    
    init_minim_hash_table(&ht);
    init_minim_object(&res, MINIM_OBJ_HASH, ht);
    return res;
}

MinimObject *minim_builtin_hashp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_HASHP(args[0]));
    return res;
}

MinimObject *minim_builtin_hash_keyp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-key?", 0, args[0]);
    
    init_minim_object(&res,
                      MINIM_OBJ_BOOL,
                      minim_hash_table_keyp(args[0]->u.ptrs.p1, args[1]));
    return res;
}

MinimObject *minim_builtin_hash_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-ref", 0, args[0]);

    res = minim_hash_table_ref(args[0]->u.ptrs.p1, args[1]);
    if (!res)
    {
        Buffer *bf;
        PrintParams pp;
        
        init_buffer(&bf);
        set_default_print_params(&pp);
        print_to_buffer(bf, args[1], env, &pp);
        res = minim_error("no value found for ~s", "hash-ref", bf->data);
    }

    return res;
}

MinimObject *minim_builtin_hash_remove(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-remove", 0, args[0]);

    copy_minim_object(&res, args[0]);
    minim_hash_table_remove(res->u.ptrs.p1, args[1]);
    return res;
}

MinimObject *minim_builtin_hash_set(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-set", 0, args[0]);

    copy_minim_object(&res, args[0]);
    minim_hash_table_add(res->u.ptrs.p1, args[1], args[2]);
    return res;
}

MinimObject *minim_builtin_hash_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-set!", 0, args[0]);

    minim_hash_table_add(args[0]->u.ptrs.p1, args[1], args[2]);
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_hash_removeb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash-remove!", 0, args[0]);

    minim_hash_table_remove(args[0]->u.ptrs.p1, args[1]);
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_hash_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!MINIM_OBJ_HASHP(args[0]))
        return minim_argument_error("hash table", "hash->list", 0, args[0]);

    return minim_hash_table_to_list(args[0]->u.ptrs.p1);
}

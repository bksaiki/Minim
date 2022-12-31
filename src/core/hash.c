#include "minimpriv.h"

#define new_bucket(b, k, v, n)                      \
{                                                   \
    (b) = GC_alloc(sizeof(MinimHashBucket));        \
    (b)->key = (k);                                 \
    (b)->val = (v);                                 \
    (b)->next = (n);                                \
    (n) = (b);                                      \
}

uint32_t hash_bytes(const void* data, size_t len)
{
    uint32_t hash = 5381;
    const char *str = (const char*) data;

    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    return hash;
}

// static void gc_mark_minim_symbol_table_row(void (*mrk)(void*, void*), void *gc, void *ptr)
// {
//     mrk(gc, ((MinimHashRow*) ptr)->arr);
// }

//
//  Hash table
//

void init_minim_hash_table(MinimHash **pht)
{
    MinimHash *ht = GC_alloc(sizeof(MinimHash));
    ht->alloc_ptr = &bucket_sizes[0];
    ht->alloc = *ht->alloc_ptr;
    ht->size = 0;
    ht->buckets = GC_calloc(ht->alloc, sizeof(MinimHashBucket*));
    *pht = ht;
}

void copy_minim_hash_table(MinimHash **pht, MinimHash *src)
{
    MinimHash *ht = GC_alloc(sizeof(MinimHash));
    ht->alloc_ptr = src->alloc_ptr;
    ht->alloc = src->alloc;
    ht->size = src->size;
    ht->buckets = GC_calloc(ht->alloc, sizeof(MinimHashBucket*));
    *pht = ht;

    for (size_t i = 0; i < src->alloc; ++i)
    {
        MinimHashBucket *t = NULL;
        for (MinimHashBucket *b = src->buckets[i]; b; b = b->next)
        {
            if (t == NULL)
            {
                ht->buckets[i] = GC_alloc(sizeof(MinimHashBucket));
                t = ht->buckets[i];
                t->key = b->key;
                t->val = b->val;
                t->next = NULL;
            }
            else
            {
                t->next = GC_alloc(sizeof(MinimHashBucket));
                t = t->next;
                t->key = b->key;
                t->val = b->val;
                t->next = NULL;
            }
        }
    }
}

bool minim_hash_table_eqp(MinimHash *a, MinimHash *b)
{
    if ((a->size != b->size) || (a->alloc != b->alloc))
        return false;

    for (size_t i = 0; i < a->alloc; ++i)
    {
        MinimHashBucket *ab = a->buckets[i], *bb = b->buckets[i];
        for (; ab && bb; ab = ab->next, bb = bb->next)
        {
            if (!minim_equalp(ab->key, bb->key))
                return false;

            if (!minim_equalp(ab->val, bb->val))
                return false;
        }

        if (ab || bb)
            return false;
    }

    return true;
}

static void rehash_table(MinimHash *ht)
{
    MinimHashBucket **old_buckets = ht->buckets;
    size_t old_alloc = ht->alloc;

    ++ht->alloc_ptr;
    ht->alloc = *ht->alloc_ptr;
    ht->buckets = GC_calloc(ht->alloc, sizeof(MinimHashBucket*));
    for (size_t i = 0; i < old_alloc; ++i)
    {
        for (MinimHashBucket *b = old_buckets[i]; b; b = b->next)
        {
            MinimHashBucket *nb;
            Buffer *bf = minim_obj_to_bytes(b->key);
            uint32_t j = hash_bytes(bf->data, bf->pos) % ht->alloc;
            new_bucket(nb, b->key, b->val, ht->buckets[j]);
        }
    }
}

static void minim_hash_table_add(MinimHash *ht, MinimObject *k, MinimObject *v)
{
    MinimHashBucket *nb;
    Buffer *bf;
    uint32_t i;

    if (((double)ht->size / (double)ht->alloc) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        rehash_table(ht); // rehash if too deep

    bf = minim_obj_to_bytes(k);
    i = hash_bytes(bf->data, bf->pos) % ht->alloc;
    for (MinimHashBucket *b = ht->buckets[i]; b; b = b->next)
    {
        // check if key exists first
        if (minim_equalp(k, b->key))
        {
            b->val = v;
            return;
        }
    }

    new_bucket(nb, k, v, ht->buckets[i]);
    ++ht->size;
}

static bool minim_hash_table_keyp(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t i = hash_bytes(bf->data, bf->pos) % ht->alloc;
    for (MinimHashBucket *b = ht->buckets[i]; b; b = b->next)
    {
        if (minim_equalp(k, b->key))
            return true;
    }

    return false;
}

static MinimObject *minim_hash_table_ref(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t i = hash_bytes(bf->data, bf->pos) % ht->alloc;
    for (MinimHashBucket *b = ht->buckets[i]; b; b = b->next)
    {
        if (minim_equalp(k, b->key))
            return b->val;
    }

    return NULL;
}

static void minim_hash_table_remove(MinimHash *ht, MinimObject *k)
{
    MinimHashBucket *b, *t;
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t i = hash_bytes(bf->data, bf->pos) % ht->alloc;

    for (t = NULL, b = ht->buckets[i]; b; b = b->next)
    {
        if (minim_equalp(k, b->key))
        {
            if (t == NULL)  ht->buckets[i] = b->next;
            else            t->next = b->next;

            --ht->size;
            return;
        }
    }
}

static MinimObject *minim_hash_table_to_list(MinimHash *ht)
{
    MinimObject *head, *it;
    
    it = NULL;
    head = NULL;
    for (size_t i = 0; i < ht->alloc; ++i)
    {
        for (MinimHashBucket *b = ht->buckets[i]; b; b = b->next)
        {
            if (!head)
            {
                head = minim_cons(minim_cons(b->key, b->val), minim_null);
                it = head;
            }
            else
            {
                MINIM_CDR(it) = minim_cons(minim_cons(b->key, b->val), minim_null);
                it = MINIM_CDR(it);
            }
        }
    }

    if (head)
    {
        MINIM_CDR(it) = minim_null;
        return head;
    }
    else
    {
        return minim_null;
    }
}

//
//  Builtins
//

MinimObject *minim_builtin_hash(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimHash *ht;

    init_minim_hash_table(&ht);
    return minim_hash_table(ht);
}

MinimObject *minim_builtin_hashp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_HASHP(args[0]));
}

MinimObject *minim_builtin_hash_keyp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-key?", 0, args[0]));
    
    return to_bool(minim_hash_table_keyp(MINIM_HASH_TABLE(args[0]), args[1]));
}

MinimObject *minim_builtin_hash_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-ref", 0, args[0]));

    res = minim_hash_table_ref(MINIM_HASH_TABLE(args[0]), args[1]);
    if (!res)
    {
        Buffer *bf;
        PrintParams pp;
        
        init_buffer(&bf);
        set_default_print_params(&pp);
        print_to_buffer(bf, args[1], env, &pp);
        THROW(env, minim_error("no value found for ~s", "hash-ref", bf->data));
    }

    return res;
}

MinimObject *minim_builtin_hash_remove(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimHash *ht;

    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-remove", 0, args[0]));

    copy_minim_hash_table(&ht, MINIM_HASH_TABLE(args[0]));
    minim_hash_table_remove(ht, args[1]);
    return minim_hash_table(ht);
}

MinimObject *minim_builtin_hash_set(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimHash *ht;

    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-set", 0, args[0]));

    copy_minim_hash_table(&ht, MINIM_HASH_TABLE(args[0]));
    minim_hash_table_add(ht, args[1], args[2]);
    return minim_hash_table(ht);
}

MinimObject *minim_builtin_hash_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-set!", 0, args[0]));

    minim_hash_table_add(MINIM_HASH_TABLE(args[0]), args[1], args[2]);
    return minim_void;
}

MinimObject *minim_builtin_hash_removeb(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash-remove!", 0, args[0]));

    minim_hash_table_remove(MINIM_HASH_TABLE(args[0]), args[1]);
    return minim_void;
}

MinimObject *minim_builtin_hash_to_list(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_HASHP(args[0]))
        THROW(env, minim_argument_error("hash table", "hash->list", 0, args[0]));

    return minim_hash_table_to_list(MINIM_HASH_TABLE(args[0]));
}

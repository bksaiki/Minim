#include "minimpriv.h"

uint32_t hash_bytes(const void* data, size_t len)
{
    uint32_t hash = 5381;
    const char *str = (const char*) data;

    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    return hash;
}

static void gc_minim_hash_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((MinimHash*) ptr)->arr);
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
            ht->arr[i].arr[j] = src->arr[i].arr[j];
    }
}

bool minim_hash_table_eqp(MinimHash *a, MinimHash *b)
{
    if ((a->size != b->size) || (a->elems != b->elems))
        return false;

    for (size_t i = 0; i < a->size; ++i)
    {
        if (a->arr[i].len != b->arr[i].len)
            return false;

        for (size_t j = 0; j < a->arr[i].len; ++j)
        {
            if (!minim_equalp(a->arr[i].arr[j], b->arr[i].arr[j]))
                return false;
        }
    }

    return true;
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
            hash = hash_bytes(bf->data, bf->pos);
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
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos);
    uint32_t reduc = hash % ht->size;

    ++ht->elems;
    if (((double)ht->elems / (double)ht->size) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        rehash_table(ht); // rehash if too deep

    if (ht->arr[reduc].len == 0)
    {
        ht->arr[reduc].arr = GC_realloc(ht->arr[reduc].arr, sizeof(MinimObject*));
        ht->arr[reduc].len = 1;
        ht->arr[reduc].arr[0] = minim_cons(k, v);
    }
    else
    {      
        // Check if key already exists
        for (size_t i = 0; i < ht->arr[reduc].len; ++i)
        {
            if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            {
                MINIM_CDR(ht->arr[reduc].arr[i]) = v;
                return;
            }
        }

        ht->arr[reduc].arr = GC_realloc(ht->arr[reduc].arr, (ht->arr[reduc].len + 1) * sizeof(MinimObject*));
        ht->arr[reduc].arr[ht->arr[reduc].len] = minim_cons(k, v);
        ++ht->arr[reduc].len;
    }
}

static bool minim_hash_table_keyp(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos);
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
    uint32_t hash = hash_bytes(bf->data, bf->pos);
    uint32_t reduc = hash % ht->size;

    for (size_t i = 0; i < ht->arr[reduc].len; ++i)
    {
        if (minim_equalp(k, MINIM_CAR(ht->arr[reduc].arr[i])))
            cp = MINIM_CDR(ht->arr[reduc].arr[i]);
    }

    return cp;
}

static void minim_hash_table_remove(MinimHash *ht, MinimObject *k)
{
    Buffer *bf = minim_obj_to_bytes(k);
    uint32_t hash = hash_bytes(bf->data, bf->pos);
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
    MinimObject *head, *it;
    
    head = NULL;
    for (size_t i = 0; i < ht->size; ++i)
    {
        for (size_t j = 0; j < ht->arr[i].len; ++j)
        {
            if (!head)
            {
                head = minim_cons(ht->arr[i].arr[j], NULL);
                it = head;
            }
            else
            {
                MINIM_CDR(it) = minim_cons(ht->arr[i].arr[j], NULL);
                it = MINIM_CDR(it);
            }
        }
    }

    if (!head)
        return minim_null;

    MINIM_CDR(it) = minim_null;
    return head;
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

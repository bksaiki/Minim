#include "minimpriv.h"

#define MINIM_TABLE_LOAD_FACTOR     0.75
#define start_size_ptr              (&bucket_sizes[0])

#define new_bucket(b, k, v, n)                          \
{                                                       \
    (b) = GC_alloc(sizeof(MinimSymbolTableBucket));     \
    (b)->key = (k);                                     \
    (b)->val = (v);                                     \
    (b)->next = (n);                                    \
    (n) = (b);                                          \
}

static void gc_mark_minim_symbol_table(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((MinimSymbolTable*) ptr)->buckets);
}

void init_minim_symbol_table(MinimSymbolTable **ptable)
{
    MinimSymbolTable *table = GC_alloc_opt(sizeof(MinimSymbolTable), NULL, gc_mark_minim_symbol_table);
    table->alloc_ptr = start_size_ptr;
    table->alloc = *table->alloc_ptr;
    table->size = 0;
    table->buckets = GC_calloc(table->alloc, sizeof(MinimSymbolTableBucket*));
    *ptable = table;
}

static void minim_symbol_table_rehash(MinimSymbolTable *table)
{
    MinimSymbolTableBucket **old_buckets;
    size_t old_size;

    old_buckets = table->buckets;
    old_size = table->alloc;
    ++table->alloc_ptr;
    table->alloc = *table->alloc_ptr;
    table->buckets = GC_calloc(table->alloc, sizeof(MinimSymbolTableBucket*));

    for (size_t i = 0; i < old_size; ++i)
    {
        for (MinimSymbolTableBucket *b = old_buckets[i]; b; b = b->next)
        {
            MinimSymbolTableBucket *nb;
            size_t h, j;

            h = hash_symbol(b->key);
            j = h % table->alloc;
            new_bucket(nb, b->key, b->val, table->buckets[j]);
        }
    }
}

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, MinimObject *obj)
{
    size_t h = hash_symbol(name);
    if (!minim_symbol_table_get2(table, name, h))
        minim_symbol_table_add2(table, name, h, obj);
}

void minim_symbol_table_add2(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj)
{
    MinimSymbolTableBucket *nb;
    size_t i;

    if (((double)table->size / (double)table->alloc) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        minim_symbol_table_rehash(table);

    // just make a new bucket, don't check for an existing one
    i = hash % table->alloc;
    new_bucket(nb, (char*) name, obj, table->buckets[i]);
    ++table->size;
}

int minim_symbol_table_set(MinimSymbolTable *table, const char *name, MinimObject *obj)
{
    return minim_symbol_table_set2(table, name, hash_symbol(name), obj);
}

int minim_symbol_table_set2(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj)
{
    size_t i = hash % table->alloc;
    for (MinimSymbolTableBucket *b = table->buckets[i]; b; b = b->next)
    {
        if (b->key == name)     // name exists
        {
            b->val = obj;
            return 1;
        }
    }

    return 0;
}

MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name)
{
    return minim_symbol_table_get2(table, name, hash_symbol(name));
}

MinimObject *minim_symbol_table_get2(MinimSymbolTable *table, const char *name, size_t hash)
{
    size_t i = hash % table->alloc;
    for (MinimSymbolTableBucket *b = table->buckets[i]; b; b = b->next)
    {
        if (b->key == name)     // name exists
            return b->val;
    }
    
    return NULL;
}

MinimObject *minim_symbol_table_remove(MinimSymbolTable *table, const char *name)
{
    MinimSymbolTableBucket *trailing;
    size_t i;

    trailing = NULL;
    i = hash_symbol(name) % table->alloc;
    for (MinimSymbolTableBucket *b = table->buckets[i]; b; b = b->next)
    {
        if (b->key == name)
        {
            if (trailing)   trailing->next = b->next;
            else            table->buckets[i] = b->next;

            --table->size;
            return b->val;
        }

        trailing = b;   
    }
    
    return NULL;
}

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj)
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (MinimSymbolTableBucket *b = table->buckets[i]; b; b = b->next)
        {
            if (minim_equalp(obj, b->val))
                return b->key;
        }
    }

    return NULL;
}

void minim_symbol_table_merge(MinimSymbolTable *dest, MinimSymbolTable *src)
{
    for (size_t i = 0; i < src->alloc; ++i)
    {
        for (MinimSymbolTableBucket *b = src->buckets[i]; b; b = b->next)
        {
            size_t h = hash_symbol(b->key);
            if (minim_symbol_table_get2(dest, b->key, h))
                minim_symbol_table_set2(dest, b->key, h, b->val);
            else
                minim_symbol_table_add2(dest, b->key, h, b->val);
        }
    }
}

void minim_symbol_table_merge2(MinimSymbolTable *dest,
                               MinimSymbolTable *src,
                               MinimObject *(*merge)(MinimObject *, MinimObject *),
                               MinimObject *(*add)(MinimObject *))
{
    for (size_t i = 0; i < src->alloc; ++i)
    {
        for (MinimSymbolTableBucket *b = src->buckets[i]; b; b = b->next)
        {
            MinimObject *v;
            size_t h;

            h = hash_symbol(b->key);
            v = minim_symbol_table_get2(dest, b->key, h);
            if (v)      minim_symbol_table_set2(dest, b->key, h, merge(v, b->val));
            else        minim_symbol_table_add2(dest, b->key, h, add(b->val));
        }
    } 
}

void minim_symbol_table_for_each(MinimSymbolTable *table, void (*func)(const char *, MinimObject *))
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (MinimSymbolTableBucket *b = table->buckets[i]; b; b = b->next)
            func(b->key, b->val);
    }
}

static size_t gensym_counter = 1;

char *gensym_unique(const char *prefix)
{
    char *sym;
    size_t len;

    len = strlen(prefix) + (gensym_counter / 10) + 1;
    sym = GC_alloc_atomic(len * sizeof(char));
    snprintf(sym, len, "%s%zu", prefix, gensym_counter++);
    return sym;
}

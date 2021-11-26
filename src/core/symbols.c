#include "minimpriv.h"

static void gc_mark_minim_symbol_table(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((MinimSymbolTable*) ptr)->rows);
}

void init_minim_symbol_table(MinimSymbolTable **ptable)
{
    MinimSymbolTable *table = GC_alloc_opt(sizeof(MinimSymbolTable), NULL, gc_mark_minim_symbol_table);
    table->alloc = MINIM_DEFAULT_SYMBOL_TABLE_SIZE;
    table->size = 0;
    table->rows = GC_calloc(MINIM_DEFAULT_SYMBOL_TABLE_SIZE, sizeof(MinimSymbolTableRow));
    *ptable = table;
}

void copy_minim_symbol_table(MinimSymbolTable **ptable, MinimSymbolTable *src)
{
    MinimSymbolTable *table = GC_alloc_opt(sizeof(MinimSymbolTable), NULL, gc_mark_minim_symbol_table);
    table->alloc = src->alloc;
    table->size = src->size;
    table->rows = GC_alloc(src->alloc * sizeof(MinimSymbolTableRow));
    *ptable = table;

    for (size_t i = 0; i < table->alloc; ++i)
    {
        if (src->rows[i].length == 0)
        {
            table->rows[i].names = NULL;
            table->rows[i].ptrs = NULL;
            table->rows[i].vals = NULL;
            table->rows[i].length = 0;
        }
        else
        {
            table->rows[i].names = GC_alloc(src->rows[i].length * sizeof(char*));
            table->rows[i].ptrs = GC_alloc(src->rows[i].length * sizeof(uintptr_t));
            table->rows[i].vals = GC_alloc(src->rows[i].length * sizeof(MinimObject*));
            table->rows[i].length = src->rows[i].length;

            for (size_t j = 0; j < src->rows[i].length; ++j)
            {
                table->rows[i].names[j] = src->rows[i].names[j];
                table->rows[i].ptrs[j] = src->rows[i].ptrs[j];
                table->rows[i].vals[j] = src->rows[i].vals[j];
            }
        }
    }
}

static void minim_symbol_table_rehash(MinimSymbolTable *table)
{
    MinimSymbolTableRow *rows;
    size_t size, hash, idx;

    size = 2 * table->alloc;
    rows = GC_calloc(size, sizeof(MinimSymbolTableRow));

    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
        {
            hash = hash_symbol(table->rows[i].names[j]);
            idx = hash % size;

            ++rows[idx].length;
            rows[idx].names = GC_realloc(rows[idx].names, rows[idx].length * sizeof(char*));
            rows[idx].ptrs = GC_realloc(rows[idx].ptrs, rows[idx].length * sizeof(uintptr_t));
            rows[idx].vals = GC_realloc(rows[idx].vals, rows[idx].length * sizeof(MinimObject*));

            rows[idx].names[rows[idx].length - 1] = table->rows[i].names[j];
            rows[idx].ptrs[rows[idx].length - 1] = table->rows[i].ptrs[j];
            rows[idx].vals[rows[idx].length - 1] = table->rows[i].vals[j];
        }
    }

    table->rows = rows;
    table->alloc = size;
}

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, MinimObject *obj)
{
    size_t h = hash_symbol(name);
    return minim_symbol_table_add2(table, name, h, obj);
}

void minim_symbol_table_add2(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj)
{
    size_t idx;

    if (((double)table->size / (double)table->alloc) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        minim_symbol_table_rehash(table);

    idx = hash % table->alloc;
    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (table->rows[idx].ptrs[i] == ((uintptr_t) name)) // name exists
        {
            table->rows[idx].vals[i] = obj;
            return;
        }
    }

    ++table->size;
    ++table->rows[idx].length;
    table->rows[idx].names = GC_realloc(table->rows[idx].names, table->rows[idx].length * sizeof(char*));
    table->rows[idx].ptrs = GC_realloc(table->rows[idx].ptrs, table->rows[idx].length * sizeof(uintptr_t));
    table->rows[idx].vals = GC_realloc(table->rows[idx].vals, table->rows[idx].length * sizeof(MinimObject*));

    table->rows[idx].names[table->rows[idx].length - 1] = GC_alloc_atomic((strlen(name) + 1) * sizeof(char));
    table->rows[idx].ptrs[table->rows[idx].length - 1] = (uintptr_t) name;
    table->rows[idx].names[table->rows[idx].length - 1] = (char*) name;     // unsafe const cast
    table->rows[idx].vals[table->rows[idx].length - 1] = obj;
}

int minim_symbol_table_set(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj)
{
    size_t idx;

    if (((double)table->size / (double)table->alloc) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        minim_symbol_table_rehash(table);

    idx = hash % table->alloc;
    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (table->rows[idx].ptrs[i] == ((uintptr_t) name)) // name exists
        {
            table->rows[idx].vals[i] = obj;
            return 1;
        }
    }

    return 0;
}

MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name, size_t hash)
{
    size_t idx;

    idx = hash % table->alloc;
    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (table->rows[idx].ptrs[i] == ((uintptr_t) name)) // name exists
            return table->rows[idx].vals[i];
    }
    
    return NULL;
}

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj)
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
        {
            if (minim_equalp(obj, table->rows[i].vals[j]))
                return table->rows[i].names[j];
        }
    }

    return NULL;
}

void minim_symbol_table_merge(MinimSymbolTable *dest, MinimSymbolTable *src)
{
    size_t hash;

    for (size_t i = 0; i < src->alloc; ++i)
    {
        for (size_t j = 0; j < src->rows[i].length; ++j)
        {
            hash = hash_symbol(src->rows[i].names[j]); 
            if (minim_symbol_table_get(dest, src->rows[i].names[j], hash))
                minim_symbol_table_set(dest, src->rows[i].names[j], hash, src->rows[i].vals[j]);
            else
                minim_symbol_table_add2(dest, src->rows[i].names[j], hash, src->rows[i].vals[j]);
        }
    }   
}

void minim_symbol_table_merge2(MinimSymbolTable *dest,
                               MinimSymbolTable *src,
                               MinimObject *(*merge)(MinimObject *, MinimObject *),
                               MinimObject *(*add)(MinimObject *))
{
    MinimObject *val;
    size_t hash;

    for (size_t i = 0; i < src->alloc; ++i)
    {
        for (size_t j = 0; j < src->rows[i].length; ++j)
        {
            hash = hash_symbol(src->rows[i].names[j]); 
            val = minim_symbol_table_get(dest, src->rows[i].names[j], hash);
            if (val)
            {
                minim_symbol_table_set(dest, src->rows[i].names[j], hash,
                                       merge(val, src->rows[i].vals[j]));
            }
            else
            {
                minim_symbol_table_add2(dest, src->rows[i].names[j], hash,
                                       add(src->rows[i].vals[j]));
            }
        }
    }   
}

void minim_symbol_table_for_each(MinimSymbolTable *table, void (*func)(const char *, MinimObject *))
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
            func(table->rows[i].names[j], table->rows[i].vals[j]);
    }
}

#include <stdlib.h>
#include <string.h>
#include "symbols.h"

#define hashseed   ((uint32_t)  0xdeadbeef)

void init_minim_symbol_table(MinimSymbolTable **ptable)
{
    MinimSymbolTable *table = malloc(sizeof(MinimSymbolTable));
    table->alloc = MINIM_DEFAULT_SYMBOL_TABLE_SIZE;
    table->size = 0;
    table->rows = malloc(MINIM_DEFAULT_SYMBOL_TABLE_SIZE * sizeof(MinimSymbolTableRow));
    *ptable = table;

    for (size_t i = 0; i < table->alloc; ++i)
    {
        table->rows[i].names = NULL;
        table->rows[i].vals = NULL;
        table->rows[i].length = 0;
    }
}

void copy_minim_symbol_table(MinimSymbolTable **ptable, MinimSymbolTable *src)
{
    MinimSymbolTable *table = malloc(sizeof(MinimSymbolTable));
    table->alloc = src->alloc;
    table->size = src->size;
    table->rows = malloc(src->alloc * sizeof(MinimSymbolTableRow));
    *ptable = table;

    for (size_t i = 0; i < table->alloc; ++i)
    {
        if (src->rows[i].length == 0)
        {
            table->rows[i].names = NULL;
            table->rows[i].vals = NULL;
            table->rows[i].length = 0;
        }
        else
        {
            table->rows[i].names = malloc(src->rows[i].length * sizeof(char*));
            table->rows[i].vals = malloc(src->rows[i].length * sizeof(MinimSymbolEntry));
            table->rows[i].length = src->rows[i].length;

            for (size_t j = 0; j < src->rows[i].length; ++j)
            {
                MinimSymbolEntry *it = src->rows[i].vals[j];
                MinimSymbolEntry *cp = malloc(sizeof(MinimSymbolEntry));
                MinimSymbolEntry *it2, *tmp;

                copy_minim_object(&cp->obj, it->obj);
                table->rows[i].vals[j] = cp;

                for (it2 = it->parent; it2; it2 = it->parent)
                {
                    tmp = malloc(sizeof(MinimSymbolEntry));
                    copy_minim_object(&cp->obj, it2->obj);
                    cp->parent = tmp;
                    cp = tmp;
                }

                cp->parent = NULL;
            }
        }
    }
}

void free_minim_symbol_table(MinimSymbolTable *table)
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
        {
            MinimSymbolEntry *val, *it, *tmp;
            
            val = table->rows[i].vals[i];
            it = val;
            while (it)
            {
                tmp = it->parent;
                free_minim_object(it->obj);
                free(it);
                it = tmp;
            }

            free(table->rows[i].names[j]);
        }

        if (table->rows[i].names)
        {
            free(table->rows[i].names);
            free(table->rows[i].vals);
        }
    }

    free(table->rows);
    free(table);
}

static void minim_symbol_table_rehash(MinimSymbolTable *table)
{
    MinimSymbolTableRow *rows;
    size_t size, hash, idx;

    size = 2 * table->alloc;
    rows = malloc(size * sizeof(MinimSymbolTableRow));
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
        {
            hash = hash_bytes(table->rows[i].names[j], strlen(table->rows[i].names[j]), hashseed);
            idx = hash % size;

            ++rows[idx].length;
            rows[idx].names = realloc(rows[idx].names, rows[idx].length);
            rows[idx].vals = realloc(rows[idx].vals, rows[idx].length);
            rows[idx].names[rows[idx].length - 1] = table->rows[i].names[j];
            rows[idx].vals[rows[idx].length - 1] = table->rows[i].vals[j];
        }
    }

    free(table->rows);
    table->rows = rows;
    table->alloc = size;
}

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, MinimObject *obj)
{
    MinimSymbolEntry *entry;
    size_t hash, idx, len;

    if (((double)table->size / (double)table->alloc) > MINIM_DEFAULT_HASH_TABLE_FACTOR)
        minim_symbol_table_rehash(table);

    len = strlen(name);
    hash = hash_bytes(name, len, hashseed);
    idx = hash % table->alloc;

    entry = malloc(sizeof(MinimSymbolEntry));
    entry->obj = obj;

    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (strcmp(table->rows[idx].names[i], name) == 0) // name exists
        {
            entry->parent = table->rows[idx].vals[i];
            table->rows[idx].vals[i] = entry;
            return;
        }
    }

    ++table->rows[idx].length;
    table->rows[idx].names = realloc(table->rows[idx].names, table->rows[idx].length);
    table->rows[idx].vals = realloc(table->rows[idx].vals, table->rows[idx].length);

    table->rows[idx].names[table->rows[idx].length - 1] = malloc((len + 1) * sizeof(char));
    strcpy(table->rows[idx].names[table->rows[idx].length - 1], name);
    table->rows[idx].vals[table->rows[idx].length - 1] = entry;
    entry->parent = NULL;
}

MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name)
{
    MinimObject *obj;
    size_t hash, idx;

    hash = hash_bytes(name, strlen(name), hashseed);
    idx = hash % table->alloc;

    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (strcmp(table->rows[idx].names[i], name) == 0) // name exists
        {
            ref_minim_object(&obj, table->rows[idx].vals[i]->obj);
            return obj;
        }
    }
    
    return NULL;
}

MinimObject *minim_symbol_table_peek(MinimSymbolTable *table, const char *name)
{
    size_t hash, idx;

    hash = hash_bytes(name, strlen(name), hashseed);
    idx = hash % table->alloc;

    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (strcmp(table->rows[idx].names[i], name) == 0) // name exists
            return table->rows[idx].vals[i]->obj;
    }
    
    return NULL;
}
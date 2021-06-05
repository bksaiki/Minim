#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
#include "hash.h"
#include "symbols.h"

#define hashseed   ((uint32_t)  0xdeadbeef)

static void gc_mark_minim_symbol_table(void (*mrk)(void*, void*), void *gc, void *ptr) {
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
    MinimSymbolTable *table = GC_alloc(sizeof(MinimSymbolTable));
    table->alloc = src->alloc;
    table->size = src->size;
    table->rows = GC_alloc(src->alloc * sizeof(MinimSymbolTableRow));
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
            table->rows[i].names = GC_alloc(src->rows[i].length * sizeof(char*));
            table->rows[i].vals = GC_alloc(src->rows[i].length * sizeof(MinimSymbolEntry*));
            table->rows[i].length = src->rows[i].length;

            for (size_t j = 0; j < src->rows[i].length; ++j)
            {
                MinimSymbolEntry *it = src->rows[i].vals[j];
                MinimSymbolEntry *cp = GC_alloc(sizeof(MinimSymbolEntry));
                MinimSymbolEntry *tmp;

                table->rows[i].names[j] = GC_alloc((strlen(src->rows[i].names[j]) + 1) * sizeof(char));
                strcpy(table->rows[i].names[j], src->rows[i].names[j]);

                copy_minim_object(&cp->obj, it->obj);
                table->rows[i].vals[j] = cp;

                for (it = it->parent; it; it = it->parent)
                {
                    tmp = GC_alloc(sizeof(MinimSymbolEntry));
                    copy_minim_object(&tmp->obj, it->obj);
                    cp->parent = tmp;
                    cp = tmp;
                }

                cp->parent = NULL;
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
            hash = hash_bytes(table->rows[i].names[j], strlen(table->rows[i].names[j]), hashseed);
            idx = hash % size;

            ++rows[idx].length;
            rows[idx].names = GC_realloc(rows[idx].names, rows[idx].length * sizeof(char*));
            rows[idx].vals = GC_realloc(rows[idx].vals, rows[idx].length * sizeof(MinimSymbolEntry*));
            rows[idx].names[rows[idx].length - 1] = table->rows[i].names[j];
            rows[idx].vals[rows[idx].length - 1] = table->rows[i].vals[j];
        }
    }

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

    entry = GC_alloc(sizeof(MinimSymbolEntry));
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

    ++table->size;
    ++table->rows[idx].length;
    table->rows[idx].names = GC_realloc(table->rows[idx].names, table->rows[idx].length * sizeof(char*));
    table->rows[idx].vals = GC_realloc(table->rows[idx].vals, table->rows[idx].length * sizeof(MinimSymbolEntry*));

    table->rows[idx].names[table->rows[idx].length - 1] = GC_alloc((len + 1) * sizeof(char));
    strcpy(table->rows[idx].names[table->rows[idx].length - 1], name);
    table->rows[idx].vals[table->rows[idx].length - 1] = entry;
    entry->parent = NULL;
}

MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name)
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

bool minim_symbol_table_pop(MinimSymbolTable *table, const char *name)
{
    size_t hash, idx;

    hash = hash_bytes(name, strlen(name), hashseed);
    idx = hash % table->alloc;

    for (size_t i = 0; i < table->rows[idx].length; ++i)
    {
        if (strcmp(table->rows[idx].names[i], name) == 0) // name exists
        {
            if (table->rows[idx].vals[i]->parent)
            {
                MinimSymbolEntry *tmp = table->rows[idx].vals[i]->parent;
                table->rows[idx].vals[i] = tmp;
            }
            else
            {
                if (i != table->rows[idx].length - 1)
                {
                    table->rows[idx].names[i] = table->rows[idx].names[table->rows[idx].length - 1];
                    table->rows[idx].vals[i] = table->rows[idx].vals[table->rows[idx].length - 1];
                }
                
                --table->rows[idx].length;
                if (table->rows[idx].length == 0)
                {
                    --table->size;
                }
                else
                {
                    table->rows[idx].names = GC_realloc(table->rows[idx].names, table->rows[idx].length * sizeof(char*));
                    table->rows[idx].vals = GC_realloc(table->rows[idx].vals, table->rows[idx].length * sizeof(MinimSymbolEntry*));
                }
            }
        }
    }
    
    return NULL;
}

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj)
{
    for (size_t i = 0; i < table->alloc; ++i)
    {
        for (size_t j = 0; j < table->rows[i].length; ++j)
        {
            for (MinimSymbolEntry *it = table->rows[i].vals[j]; it; it = it->parent)
            {
                if (minim_equalp(obj, it->obj))
                    return table->rows[i].names[j];
            }
        }
    }

    return NULL;
}

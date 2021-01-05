#include "stdlib.h"
#include "string.h"
#include "symbols.h"

void init_minim_symbol_table(MinimSymbolTable **ptable)
{
    MinimSymbolTable *table = malloc(sizeof(MinimSymbolTable));
    table->alloc = MINIM_DEFAULT_SYMBOL_TABLE_SIZE;
    table->size = 0;
    table->rows = malloc(MINIM_DEFAULT_SYMBOL_TABLE_SIZE * sizeof(MinimSymbolTableRow*));
    *ptable = table;

    for (size_t i = 0; i < table->alloc; ++i)
    {
        table->rows[i] = malloc(sizeof(MinimSymbolTableRow));
        table->rows[i]->names = NULL;
        table->rows[i]->vals = NULL;
        table->rows[i]->length = 0;
    }
}

void copy_minim_symbol_table(MinimSymbolTable **ptable, MinimSymbolTable *src)
{
    MinimSymbolTable *table = malloc(sizeof(MinimSymbolTable));
    table->alloc = src->alloc;
    table->size = src->size;
    table->rows = malloc(src->alloc * sizeof(MinimSymbolTableRow*));
    *ptable = table;

    for (size_t i = 0; i < table->alloc; ++i)
    {
        table->rows[i] = malloc(sizeof(MinimSymbolTableRow));
        if (src->rows[i]->length == 0)
        {
            table->rows[i]->names = NULL;
            table->rows[i]->vals = NULL;
            table->rows[i]->length = 0;
        }
        else
        {
            table->rows[i]->names = malloc(src->rows[i]->length * sizeof(char*));
            table->rows[i]->vals = malloc(src->rows[i]->length * sizeof(MinimSymbolEntry));
            table->rows[i]->length = src->rows[i]->length;

            for (size_t j = 0; j < src->rows[i]->length; ++j)
            {
                MinimSymbolEntry *it = src->rows[i]->vals[j];
                MinimSymbolEntry *cp = malloc(sizeof(MinimSymbolEntry));
                MinimSymbolEntry *it2, *tmp;

                copy_minim_object(&cp->obj, it->obj);
                table->rows[i]->vals[j] = cp;

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
        for (size_t j = 0; j < table->rows[i]->length; ++j)
        {
            MinimSymbolEntry *val, *it, *tmp;
            
            val = table->rows[i]->vals[i];
            it = val;
            while (it)
            {
                tmp = it->parent;
                free_minim_object(it->obj);
                free(it);
                it = tmp;
            }

            free(table->rows[i]->names[j]);
        }

        if (table->rows[i]->names)
        {
            free(table->rows[i]->names);
            free(table->rows[i]->vals);
        }

        free(table->rows[i]);
    }

    free(table->rows);
    free(table);
}
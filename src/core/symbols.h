#ifndef _MINIM_SYMBOLS_H_
#define _MINIM_SYMBOLS_H_

#include "object.h"

#define MINIM_DEFAULT_SYMBOL_TABLE_SIZE 64

struct MinimSymbolEntry
{
    struct MinimSymbolEntry *parent;
    MinimObject *obj;
} typedef MinimSymbolEntry;

struct MinimSymbolTableRow
{
    char **names;
    MinimSymbolEntry **vals;
    size_t length;
} typedef MinimSymbolTableRow;

struct MinimSymbolTable
{
    MinimSymbolTableRow *rows;
    size_t size, alloc;
} typedef MinimSymbolTable;

void init_minim_symbol_table(MinimSymbolTable **ptable);
void copy_minim_symbol_table(MinimSymbolTable **ptable, MinimSymbolTable *src);
void free_minim_symbol_table(MinimSymbolTable *table);

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, MinimObject *obj);
MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name);
MinimObject *minim_symbol_table_peek(MinimSymbolTable *table, const char *name);
bool minim_symbol_table_pop(MinimSymbolTable *table, const char *name);

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj);



#endif

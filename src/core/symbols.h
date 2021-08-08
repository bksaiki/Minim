#ifndef _MINIM_SYMBOLS_H_
#define _MINIM_SYMBOLS_H_

#include "object.h"

#define MINIM_DEFAULT_SYMBOL_TABLE_SIZE 10

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

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj);
int minim_symbol_table_set(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj);
MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name, size_t hash);
bool minim_symbol_table_pop(MinimSymbolTable *table, const char *name);

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj);

void minim_symbol_table_merge(MinimSymbolTable *dest, MinimSymbolTable *src);

void minim_symbol_table_for_each(MinimSymbolTable *table, void (*func)(const char *, MinimObject *));

#endif

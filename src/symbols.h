#ifndef _MINIM_SYMBOLS_H_
#define _MINIM_SYMBOLS_H_

#include "hash.h"

#define MINIM_DEFAULT_SYMBOL_TABLE_SIZE 128

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
    MinimSymbolTableRow **rows;
    size_t size;
    size_t alloc;
} typedef MinimSymbolTable;

// Initialization / Destruction

void init_minim_symbol_table(MinimSymbolTable **ptable);
void copy_minim_symbol_table(MinimSymbolTable **ptable, MinimSymbolTable *src);
void free_minim_symbol_table(MinimSymbolTable *table);

#endif
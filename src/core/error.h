#ifndef _MINIM_ERROR_H_
#define _MINIM_ERROR_H_

#include "../common/read.h"
#include "env.h"

struct MinimErrorTrace
{
    struct MinimErrorTrace *next;
    SyntaxLoc *loc;
    char *name;
    bool multiple;
} typedef MinimErrorTrace;

struct MinimErrorDescTable
{
    char **keys;
    char **vals;
    size_t len;
} typedef MinimErrorDescTable;

struct MinimError
{
    MinimErrorTrace *top;
    MinimErrorTrace *bottom;
    MinimErrorDescTable *table;
    char *where;
    char *msg;
} typedef MinimError;

// *** Minim Error Trace *** //

void init_minim_error_trace(MinimErrorTrace **ptrace, SyntaxLoc* loc, const char* name);
void copy_minim_error_trace(MinimErrorTrace **ptrace, MinimErrorTrace *src);
void free_minim_error_trace(MinimErrorTrace *trace);

// *** Descriptor Table *** //

void init_minim_error_desc_table(MinimErrorDescTable **ptable, size_t len);
void copy_minim_error_desc_table(MinimErrorDescTable **ptable, MinimErrorDescTable *src);
void free_minim_error_desc_table(MinimErrorDescTable *table);
void minim_error_desc_table_set(MinimErrorDescTable *table, size_t idx, const char *key, const char *val);

// *** Minim Error *** //

void init_minim_error(MinimError **perr, const char *msg, const char *where);
void copy_minim_error(MinimError **perr, MinimError *src);
void free_minim_error(MinimError *err);
void minim_error_add_trace(MinimError *err, SyntaxLoc *loc, const char* name);

// *** Errors *** //

MinimObject *minim_argument_error(const char *pred, const char *where, size_t pos, MinimObject *val);
MinimObject *minim_error(const char *msg, const char *where, ...);

#endif
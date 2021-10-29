#ifndef _MINIM_ERROR_H_
#define _MINIM_ERROR_H_

#include "env.h"
#include "syntax.h"

struct MinimErrorTrace
{
    struct MinimErrorTrace *next;
    SyntaxLoc *loc;
    char *name;
    bool multiple;
} typedef MinimErrorTrace;

struct MinimErrorDescTable
{
    char **keys, **vals;
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

// *** Descriptor Table *** //

void init_minim_error_desc_table(MinimErrorDescTable **ptable, size_t len);
void copy_minim_error_desc_table(MinimErrorDescTable **ptable, MinimErrorDescTable *src);
void minim_error_desc_table_set(MinimErrorDescTable *table, size_t idx, const char *key, const char *val);

// *** Minim Error *** //

void init_minim_error(MinimError **perr, const char *msg, const char *where);
void minim_error_add_trace(MinimError *err, SyntaxLoc *loc, const char* name);

// *** Errors *** //

MinimObject *minim_syntax_error(const char *msg, const char *where, MinimObject *expr, MinimObject *subexpr);
MinimObject *minim_argument_error(const char *pred, const char *where, size_t pos, MinimObject *val);
MinimObject *minim_arity_error(const char *where, size_t min, size_t max, size_t actual);
MinimObject *minim_values_arity_error(const char *where, size_t expected, size_t actual, MinimObject *expr);
MinimObject *minim_error(const char *msg, const char *where, ...);

NORETURN
void throw_minim_error(MinimEnv *env, MinimObject *err);

#define THROW(e, x)    throw_minim_error(e, x)

#endif

#ifndef _MINIM_ERROR_H_
#define _MINIM_ERROR_H_

#include "env.h"

struct MinimErrorTrace
{
    struct MinimErrorTrace *next;
    char *name;
    bool multiple;
} typedef MinimErrorTrace;

struct MinimError
{
    MinimErrorTrace *trace;
    char *where;
    char *msg;
} typedef MinimError;

// *** Minim Error Trace *** //

void init_minim_error_trace(MinimErrorTrace **ptrace, const char* name);
void free_minim_error_trace(MinimErrorTrace *trace);

// *** Minim Error *** //

void init_minim_error(MinimError **perr, const char *msg, const char *where);
void free_minim_error(MinimError *err);
void minim_error_add_trace(MinimError *err, const char *name);

#endif
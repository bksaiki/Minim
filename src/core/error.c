#include <stdlib.h>
#include <string.h>
#include "error.h"

void init_minim_error_trace(MinimErrorTrace **ptrace, const char* name)
{
    MinimErrorTrace *trace = malloc(sizeof(MinimErrorTrace));
    trace->name = malloc((strlen(name) + 1) * sizeof(char));
    trace->multiple = false;
    trace->next = NULL;
    *ptrace = trace;

    strcpy(trace->name, name);
}

void free_minim_error_trace(MinimErrorTrace *trace)
{
    if (trace->next)    free_minim_error_trace(trace->next);
    if (trace->name)    free(trace->name);
    free(trace);
}

void init_minim_error(MinimError **perr, const char *msg, const char *where)
{
    MinimError *err = malloc(sizeof(MinimError));
    err->trace = NULL;
    err->msg = malloc((strlen(msg) + 1) * sizeof(char));
    strcpy(err->msg, msg);
    *perr = err;
    
    if (where)
    {
        err->where = malloc((strlen(where) + 1) * sizeof(char));
        strcpy(err->where, where);
    }
    else
    {
        err->where = NULL;
    }
}

void free_minim_error(MinimError *err)
{
    if (err->trace) free_minim_error_trace(err->trace);
    if (err->msg)   free(err->msg);
    if (err->where) free(err->where);
    free(err);
}

void minim_error_add_trace(MinimError *err, const char *name)
{
    MinimErrorTrace *trace;

    if (err->trace)
    {
        if (strcmp(err->trace->name, name) == 0)
        {
            err->trace->multiple = true;
        }
        else
        {
            init_minim_error_trace(&trace, name);
            trace->next = err->trace;
            err->trace = trace;
        }
    }
    else
    {
        init_minim_error_trace(&trace, name);
        err->trace = trace;
    }
}
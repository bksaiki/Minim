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

void copy_minim_error_trace(MinimErrorTrace **ptrace, MinimErrorTrace *src)
{
    MinimErrorTrace *trace = malloc(sizeof(MinimErrorTrace));
    trace->name = malloc((strlen(src->name) + 1) * sizeof(char));
    trace->multiple = src->multiple;
    *ptrace = trace;

    if (src->next)      copy_minim_error_trace(&trace->next, src->next);
    else                trace->next = NULL;
    strcpy(trace->name, src->name);
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
    err->msg = malloc((strlen(msg) + 1) * sizeof(char));
    strcpy(err->msg, msg);
    err->top = NULL;
    err->bottom = NULL;
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

void copy_minim_error(MinimError **perr, MinimError *src)
{
    MinimError *err = malloc(sizeof(MinimError));
    err->msg = malloc((strlen(src->msg) + 1) * sizeof(char)); 
    strcpy(err->msg, src->msg);
    *perr = err;
    
    if (src->top)
    {
        MinimErrorTrace *it;

        copy_minim_error_trace(&err->top, src->top);
        for (it = err->top; it->next; it = it->next);
        err->bottom = it;
    }
    else
    {
        err->top = NULL;
        err->bottom = NULL;
    }
    
    if (src->where)
    {
        err->where = malloc((strlen(src->where) + 1) * sizeof(char)); 
        strcpy(err->where, src->where);
    }
    else
    {
        err->where = NULL;
    }
}

void free_minim_error(MinimError *err)
{
    if (err->top)   free_minim_error_trace(err->top);
    if (err->msg)   free(err->msg);
    if (err->where) free(err->where);
    free(err);
}

void minim_error_add_trace(MinimError *err, const char *name)
{
    MinimErrorTrace *trace;

    if (err->bottom)
    {
        if (strcmp(err->bottom->name, name) == 0)
        {
            err->bottom->multiple = true;
        }
        else
        {
            init_minim_error_trace(&trace, name);
            err->bottom->next = trace;
            err->bottom = trace;
        }
    }
    else
    {
        init_minim_error_trace(&trace, name);
        err->top = trace;
        err->bottom = trace;
    }
}
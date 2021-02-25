#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "error.h"
#include "string.h"
#include "variable.h"

void init_minim_error_trace(MinimErrorTrace **ptrace, SyntaxLoc *loc, const char *name)
{
    MinimErrorTrace *trace = malloc(sizeof(MinimErrorTrace));
    copy_syntax_loc(&trace->loc, loc);
    trace->multiple = false;
    trace->next = NULL;
    *ptrace = trace;

    trace->name = malloc((strlen(name) + 1) * sizeof(char));
    strcpy(trace->name, name);
}

void copy_minim_error_trace(MinimErrorTrace **ptrace, MinimErrorTrace *src)
{
    MinimErrorTrace *trace = malloc(sizeof(MinimErrorTrace));
    copy_syntax_loc(&trace->loc, src->loc);
    trace->multiple = src->multiple;
    *ptrace = trace;

    if (src->next)      copy_minim_error_trace(&trace->next, src->next);
    else                trace->next = NULL;

    trace->name = malloc((strlen(src->name) + 1) * sizeof(char));
    strcpy(trace->name, src->name);
}

void free_minim_error_trace(MinimErrorTrace *trace)
{
    if (trace->next)    free_minim_error_trace(trace->next);
    if (trace->loc)     free_syntax_loc(trace->loc);
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

void minim_error_add_trace(MinimError *err, SyntaxLoc *loc, const char *name)
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
            init_minim_error_trace(&trace, loc, name);
            err->bottom->next = trace;
            err->bottom = trace;
        }
    }
    else
    {
        init_minim_error_trace(&trace, loc, name);
        err->top = trace;
        err->bottom = trace;
    }
}

MinimObject *minim_builtin_error(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimError *err;

    if (assert_range_argc(&res, "error", 1, 2, argc))
    {
        if (assert_generic(&res, "Execpted a string in the 1st argument of 'error'",
                            minim_symbolp(args[0]) || minim_stringp(args[0])))
        {
            if (argc == 1)
            {
                init_minim_error(&err, args[0]->data, NULL);
                init_minim_object(&res, MINIM_OBJ_ERR, err);
            }
            else
            {
                if (assert_generic(&res, "Execpted a string in the 2nd argument of 'error'",
                                    minim_symbolp(args[1]) || minim_stringp(args[1])))
                {
                    init_minim_error(&err, args[1]->data, args[0]->data);
                    init_minim_object(&res, MINIM_OBJ_ERR, err);
                }
            }
        }
    }

    return res;
}
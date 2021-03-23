#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "error.h"
#include "string.h"

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

void init_minim_error_desc_table(MinimErrorDescTable **ptable, size_t len)
{
    MinimErrorDescTable *table = malloc(sizeof(MinimErrorDescTable));

    table->keys = calloc(len, sizeof(char*));
    table->vals = calloc(len, sizeof(char*));
    table->len = len;

    *ptable = table;
}

void copy_minim_error_desc_table(MinimErrorDescTable **ptable, MinimErrorDescTable *src)
{
    MinimErrorDescTable *table = malloc(sizeof(MinimErrorDescTable));

    table->keys = calloc(src->len, sizeof(char*));
    table->vals = calloc(src->len, sizeof(char*));
    table->len = src->len;
    *ptable = table;

    for (size_t i = 0; i < src->len; ++i)
    {
        if (src->keys[i])
        {
            table->keys[i] = malloc((strlen(src->keys[i]) + 1) * sizeof(char));
            table->vals[i] = malloc((strlen(src->vals[i]) + 1) * sizeof(char));
            strcpy(table->keys[i], src->keys[i]);
            strcpy(table->vals[i], src->vals[i]);
        }
    }
}

void free_minim_error_desc_table(MinimErrorDescTable *table)
{
    for (size_t i = 0; i < table->len; ++i)
    {
        if (table->keys[i])
        {
            free(table->keys[i]);
            free(table->vals[i]);
        }
    }

    free(table->keys);
    free(table->vals);
    free(table);
}

void minim_error_desc_table_set(MinimErrorDescTable *table, size_t idx, const char *key, const char *val)
{
    if (table->keys[idx])   free(table->keys[idx]);
    if (table->keys[idx])   free(table->vals[idx]);

    table->keys[idx] = malloc((strlen(key) + 1) * sizeof(char));
    table->vals[idx] = malloc((strlen(val) + 1) * sizeof(char));
    strcpy(table->keys[idx], key);
    strcpy(table->vals[idx], val);
}

void init_minim_error(MinimError **perr, const char *msg, const char *where)
{
    MinimError *err = malloc(sizeof(MinimError));

    err->msg = malloc((strlen(msg) + 1) * sizeof(char));
    strcpy(err->msg, msg);
    err->top = NULL;
    err->bottom = NULL;
    err->table = NULL;
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

    if (src->table)
        copy_minim_error_desc_table(&err->table, src->table);
    else
        err->table = NULL;
}

void free_minim_error(MinimError *err)
{
    if (err->top)       free_minim_error_trace(err->top);
    if (err->table)     free_minim_error_desc_table(err->table);
    if (err->msg)       free(err->msg);
    if (err->where)     free(err->where);
    
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

// ************ Internals ****************

static void buffer_write_ordinal(Buffer *bf, size_t ord)
{
    size_t ld = ord;

    writeu_buffer(bf, ord);
    if (ord == 11 || ord == 12 || ord == 13)    writes_buffer(bf, "th");
    else if (ld == 1)                           writes_buffer(bf, "st");
    else if (ld == 2)                           writes_buffer(bf, "nd");
    else if (ld == 3)                           writes_buffer(bf, "rd");
    else                                        writes_buffer(bf, "th");
}

MinimObject *minim_argument_error(const char *pred, const char *where, size_t pos, MinimObject *val)
{
    MinimObject *obj;
    MinimError *err;
    size_t idx;

    init_minim_error(&err, "argument error", where);
    init_minim_error_desc_table(&err->table, 3);
    minim_error_desc_table_set(err->table, 0, "expected", pred);
    idx = 1;

    if (val)
    {
        Buffer *bf;
        MinimEnv *env;
        PrintParams pp;

        init_env(&env, NULL);
        init_buffer(&bf);
        set_default_print_params(&pp);
        print_to_buffer(bf, val, env, &pp);
        minim_error_desc_table_set(err->table, idx, "given", bf->data);

        free_env(env);
        free_buffer(bf);
        ++idx;
    }

    if (pos != SIZE_MAX)
    {
        Buffer *bf;

        init_buffer(&bf);
        buffer_write_ordinal(bf, pos + 1);
        minim_error_desc_table_set(err->table, idx, "location", bf->data);
        free_buffer(bf);
    }

    init_minim_object(&obj, MINIM_OBJ_ERR, err);
    return obj;
}

MinimObject *minim_error(const char *msg, const char *where, ...)
{
    MinimObject *obj;
    MinimError *err;
    Buffer *bf;
    va_list va;

    va_start(va, where);
    init_buffer(&bf);
    vwritef_buffer(bf, msg, va);
    va_end(va);   

    init_minim_error(&err, bf->data, where);
    init_minim_object(&obj, MINIM_OBJ_ERR, err);
    free_buffer(bf);

    return obj;
}

// ************ Builtins ****************

MinimObject *minim_builtin_error(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimError *err;

    if (!MINIM_OBJ_SYMBOLP(args[0]) && !MINIM_OBJ_STRINGP(args[0]))
    {
        res = minim_argument_error("symbol?/string?", "error", 0, args[0]);
        return res;
    }

    if (argc == 1)
    {
        init_minim_error(&err, args[0]->u.str.str, NULL);
        init_minim_object(&res, MINIM_OBJ_ERR, err);
    }
    else
    {
        if (!MINIM_OBJ_SYMBOLP(args[1]) && !MINIM_OBJ_STRINGP(args[1]))
        {
            res = minim_argument_error("symbol?/string?", "error", 1, args[1]);
            return res;
        }
    
        init_minim_error(&err, args[1]->u.str.str, args[0]->u.str.str);
        init_minim_object(&res, MINIM_OBJ_ERR, err);
    }

    return res;
}
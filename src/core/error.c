#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "string.h"

static void gc_minim_error_trace_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimErrorTrace *trace = (MinimErrorTrace*) ptr;
    mrk(gc, trace->next);
    mrk(gc, trace->loc);
    mrk(gc, trace->name);
}

static void gc_minim_error_desc_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimErrorDescTable *desc = (MinimErrorDescTable*) ptr;
    mrk(gc, desc->keys);
    mrk(gc, desc->vals);
}

void init_minim_error_trace(MinimErrorTrace **ptrace, SyntaxLoc *loc, const char *name)
{
    MinimErrorTrace *trace = GC_alloc_opt(sizeof(MinimErrorTrace), NULL, gc_minim_error_trace_mrk);
    copy_syntax_loc(&trace->loc, loc);
    trace->multiple = false;
    trace->next = NULL;
    *ptrace = trace;

    if (name)
    {
        trace->name = GC_alloc_atomic((strlen(name) + 1) * sizeof(char));
        strcpy(trace->name, name);
    }
    else
    {
        trace->name = NULL;
    }
}

void copy_minim_error_trace(MinimErrorTrace **ptrace, MinimErrorTrace *src)
{
    MinimErrorTrace *trace = GC_alloc_opt(sizeof(MinimErrorTrace), NULL, gc_minim_error_trace_mrk);
    copy_syntax_loc(&trace->loc, src->loc);
    trace->multiple = src->multiple;
    *ptrace = trace;

    if (src->next)      copy_minim_error_trace(&trace->next, src->next);
    else                trace->next = NULL;

    trace->name = GC_alloc_atomic((strlen(src->name) + 1) * sizeof(char));
    strcpy(trace->name, src->name);
}

void init_minim_error_desc_table(MinimErrorDescTable **ptable, size_t len)
{
    MinimErrorDescTable *table = GC_alloc_opt(sizeof(MinimErrorDescTable),
                                              NULL,
                                              gc_minim_error_desc_mrk);

    table->keys = GC_calloc(len, sizeof(char*));
    table->vals = GC_calloc(len, sizeof(char*));
    table->len = len;

    *ptable = table;
}

void copy_minim_error_desc_table(MinimErrorDescTable **ptable, MinimErrorDescTable *src)
{
    MinimErrorDescTable *table = GC_alloc_opt(sizeof(MinimErrorDescTable),
                                              NULL,
                                              gc_minim_error_desc_mrk);

    table->keys = GC_calloc(src->len, sizeof(char*));
    table->vals = GC_calloc(src->len, sizeof(char*));
    table->len = src->len;
    *ptable = table;

    for (size_t i = 0; i < src->len; ++i)
    {
        if (src->keys[i])
        {
            table->keys[i] = GC_alloc_atomic((strlen(src->keys[i]) + 1) * sizeof(char));
            table->vals[i] = GC_alloc_atomic((strlen(src->vals[i]) + 1) * sizeof(char));
            strcpy(table->keys[i], src->keys[i]);
            strcpy(table->vals[i], src->vals[i]);
        }
    }
}

void minim_error_desc_table_set(MinimErrorDescTable *table, size_t idx, const char *key, const char *val)
{
    table->keys[idx] = GC_alloc_atomic((strlen(key) + 1) * sizeof(char));
    table->vals[idx] = GC_alloc_atomic((strlen(val) + 1) * sizeof(char));
    strcpy(table->keys[idx], key);
    strcpy(table->vals[idx], val);
}

void init_minim_error(MinimError **perr, const char *msg, const char *where)
{
    MinimError *err = GC_alloc(sizeof(MinimError));

    err->msg = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
    strcpy(err->msg, msg);
    err->top = NULL;
    err->bottom = NULL;
    err->table = NULL;
    *perr = err;
    
    if (where)
    {
        err->where = GC_alloc_atomic((strlen(where) + 1) * sizeof(char));
        strcpy(err->where, where);
    }
    else
    {
        err->where = NULL;
    }
}

void copy_minim_error(MinimError **perr, MinimError *src)
{
    MinimError *err = GC_alloc(sizeof(MinimError));

    err->msg = GC_alloc_atomic((strlen(src->msg) + 1) * sizeof(char)); 
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
        err->where = GC_alloc_atomic((strlen(src->where) + 1) * sizeof(char)); 
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

void minim_error_add_trace(MinimError *err, SyntaxLoc *loc, const char *name)
{
    MinimErrorTrace *trace;

    if (err->bottom && err->bottom->name)
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

MinimObject *minim_syntax_error(const char *msg, const char *where, SyntaxNode *expr, SyntaxNode *subexpr)
{
    MinimObject *obj;
    MinimError *err;
    Buffer *bf;

    init_minim_error(&err, msg, where);
    init_minim_object(&obj, MINIM_OBJ_ERR, err);
    if (expr && !subexpr)
    {
        init_buffer(&bf);
        ast_to_buffer(expr, bf);
        init_minim_error_desc_table(&err->table, 1);
        minim_error_desc_table_set(err->table, 0, "in", get_buffer(bf));

        if (expr->loc)
            minim_error_add_trace(err, expr->loc, NULL);
    }
    else if (expr && subexpr)
    {
        init_buffer(&bf);
        ast_to_buffer(subexpr, bf);
        init_minim_error_desc_table(&err->table, 2);
        minim_error_desc_table_set(err->table, 0, "at", get_buffer(bf));
        init_minim_object(&obj, MINIM_OBJ_ERR, err);

        init_buffer(&bf);
        ast_to_buffer(expr, bf);
        minim_error_desc_table_set(err->table, 1, "in", get_buffer(bf));
        
        if (expr->loc)
            minim_error_add_trace(err, expr->loc, NULL);
    }

    return obj;
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

        init_env(&env, NULL, NULL);
        init_buffer(&bf);
        set_default_print_params(&pp);
        print_to_buffer(bf, val, env, &pp);
        minim_error_desc_table_set(err->table, idx, "given", bf->data);
        ++idx;
    }

    if (pos != SIZE_MAX)
    {
        Buffer *bf;

        init_buffer(&bf);
        buffer_write_ordinal(bf, pos + 1);
        minim_error_desc_table_set(err->table, idx, "location", bf->data);
    }

    init_minim_object(&obj, MINIM_OBJ_ERR, err);
    return obj;
}

MinimObject *minim_arity_error(const char *where, size_t min, size_t max, size_t actual)
{
    MinimObject *obj;
    MinimError *err;
    Buffer *bf;

    init_minim_error(&err, "arity mismatch", where);
    init_minim_error_desc_table(&err->table, 2);

    init_buffer(&bf);
    if (min == max)             writef_buffer(bf, "~u", min);
    else if (max == SIZE_MAX)   writef_buffer(bf, "at least ~u", min);
    else                        writef_buffer(bf, "between ~u and ~u", min, max);  
    minim_error_desc_table_set(err->table, 0, "expected", bf->data);
    
    reset_buffer(bf);
    writef_buffer(bf, "~u", actual);
    minim_error_desc_table_set(err->table, 1, "got", bf->data);

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
    return obj;
}

// ************ Builtins ****************

MinimObject *minim_builtin_error(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_syntax_error(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SYMBOLP(args[0]) && !MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("symbol?/string?", "syntax-error", 0, args[0]);

    if (!MINIM_OBJ_SYMBOLP(args[1]) && !MINIM_OBJ_STRINGP(args[1]))
        return minim_argument_error("symbol?/string?", "syntax-error", 1, args[1]);

    if (argc == 2)
    {
        return minim_syntax_error(MINIM_STRING(args[1]),
                                  MINIM_STRING(args[0]),
                                  NULL,
                                  NULL);
    }
    else if (argc == 3)
    {
        return minim_syntax_error(MINIM_STRING(args[1]),
                                  MINIM_STRING(args[0]),
                                  MINIM_AST(args[2]),
                                  NULL);
    }
    else
    {
        return minim_syntax_error(MINIM_STRING(args[1]),
                                  MINIM_STRING(args[0]),
                                  MINIM_AST(args[2]),
                                  MINIM_AST(args[3]));
    }
}

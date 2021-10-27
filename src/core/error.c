#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "builtin.h"
#include "error.h"
#include "jmp.h"
#include "lambda.h"
#include "number.h"
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

MinimObject *minim_syntax_error(const char *msg, const char *where, MinimObject *expr, MinimObject *subexpr)
{
    MinimObject *obj;
    MinimError *err;
    Buffer *bf;

    init_minim_error(&err, msg, where);
    obj = minim_err(err);
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
        obj = minim_err(err);

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

    return minim_err(err);
}

MinimObject *minim_arity_error(const char *where, size_t min, size_t max, size_t actual)
{
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
    return minim_err(err);
}

MinimObject *minim_values_arity_error(const char *where, size_t expected, size_t actual, MinimObject *expr)
{
    MinimError *err;
    Buffer *bf;

    init_minim_error(&err, "result arity mismatch", where);
    init_minim_error_desc_table(&err->table, (expr ? 3 : 2));

    init_buffer(&bf);
    writef_buffer(bf, "~u", expected);
    minim_error_desc_table_set(err->table, 0, "expected", get_buffer(bf));

    reset_buffer(bf);
    writef_buffer(bf, "~u", actual);
    minim_error_desc_table_set(err->table, 1, "received", get_buffer(bf));

    if (expr)
    {
        reset_buffer(bf);
        ast_to_buffer(expr, bf);
        minim_error_desc_table_set(err->table, 2, "in", get_buffer(bf));
    }
    
    return minim_err(err);
}

MinimObject *minim_error(const char *msg, const char *where, ...)
{
    MinimError *err;
    Buffer *bf;
    va_list va;

    va_start(va, where);
    init_buffer(&bf);
    vwritef_buffer(bf, msg, va);
    va_end(va);   

    init_minim_error(&err, bf->data, where);
    return minim_err(err);
}

static void fill_trace_info(MinimError *err, MinimEnv *env)
{
    if (env->callee && env->callee->name)
    {
        if (env->callee->body->loc && env->callee->name)
            minim_error_add_trace(err, env->callee->body->loc, env->callee->name);
    }

    if (env->caller)        fill_trace_info(err, env->caller);
    else if (env->parent)   fill_trace_info(err, env->parent);
}

void throw_minim_error(MinimEnv *env, MinimObject *err)
{
    if (!MINIM_OBJ_ERRORP(err))
        printf("Attempting to throw something that is not an error\n");

    if (env)
        fill_trace_info(MINIM_ERROR(err), env);
        
    minim_long_jump(minim_error_handler, NULL, 1, &err);
}

// ************ Builtins ****************

MinimObject *minim_builtin_error(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (argc == 1)
    {
        if (!MINIM_OBJ_STRINGP(args[0]))
            THROW(env, minim_argument_error("string?", "error", 0, args[0]));

        THROW(env, minim_error(MINIM_STRING(args[0]), NULL));
    }
    else
    {
        if (MINIM_OBJ_SYMBOLP(args[0]))
        {
            MinimObject *format = minim_builtin_format(env, argc - 1, &args[1]);
            THROW(env, minim_error(MINIM_STRING(format), MINIM_SYMBOL(args[0])));
        }
        else if (MINIM_OBJ_STRINGP(args[0]))
        {
            MinimObject *format = minim_builtin_format(env, argc, args);
            THROW(env, minim_error(MINIM_STRING(format), NULL));
        }
        else
        {
            THROW(env, minim_argument_error("symbol?/string?", "error", 0, args[0]));
        }
    }

    return NULL; // avoid compile errors
}

MinimObject *minim_builtin_argument_error(MinimEnv *env, size_t argc, MinimObject **args)
{
    PrintParams pp;
    MinimError *err;
    Buffer *bf;

    if (!MINIM_OBJ_SYMBOLP(args[0]))
        THROW(env, minim_argument_error("symbol?", "argument-error", 0, args[0]));

    if (!MINIM_OBJ_STRINGP(args[1]))
        THROW(env, minim_argument_error("string?", "argument-error", 1, args[1]));

    if (argc == 3)
    {
        init_minim_error(&err, "argument error", MINIM_SYMBOL(args[0]));
        init_minim_error_desc_table(&err->table, 2);
        minim_error_desc_table_set(err->table, 0, "expected", MINIM_STRING(args[1]));

        init_buffer(&bf);
        set_default_print_params(&pp);
        print_to_buffer(bf, args[2], env, &pp);
        minim_error_desc_table_set(err->table, 1, "given", get_buffer(bf));        
    }
    else
    {
        size_t idx;

        if (!minim_exact_nonneg_intp(args[2]))
            THROW(env, minim_argument_error("exact nonnegative integer?", "argument-error", 2, args[2]));

        set_default_print_params(&pp);
        idx = MINIM_NUMBER_TO_UINT(args[2]);
        if (argc < 4 + idx)
            THROW(env, minim_error("not enough arguments provided", "argument-error"));
  
        init_minim_error(&err, "argument error", MINIM_SYMBOL(args[0]));
        init_minim_error_desc_table(&err->table, 4);
        minim_error_desc_table_set(err->table, 0, "expected", MINIM_STRING(args[1]));

        init_buffer(&bf);
        print_to_buffer(bf, args[3 + idx], env, &pp);
        minim_error_desc_table_set(err->table, 1, "given", get_buffer(bf));

        clear_buffer(bf);
        print_to_buffer(bf, args[2], env, &pp);
        minim_error_desc_table_set(err->table, 2, "position", get_buffer(bf));
        
        clear_buffer(bf);
        for (size_t i = 3; i < argc; ++i)
        {
            if (i - 3 != idx)
            {
                writef_buffer(bf, "\n;    ");
                print_to_buffer(bf, args[i], env, &pp);
            }
        }

        minim_error_desc_table_set(err->table, 3, "other arguments", get_buffer(bf));
    }

    THROW(env, minim_err(err));
    return minim_null;
}

MinimObject *minim_builtin_arity_error(MinimEnv *env, size_t argc, MinimObject **args)
{
    PrintParams pp;
    MinimError *err;
    Buffer *bf;

    if (!MINIM_OBJ_SYMBOLP(args[0]))
        THROW(env, minim_argument_error("symbol?", "arity-error", 0, args[0]));

    if (minim_exact_nonneg_intp(args[1]))
    {
        size_t exact = MINIM_NUMBER_TO_UINT(args[1]);

        init_minim_error(&err, "arity error", MINIM_SYMBOL(args[0]));
        init_minim_error_desc_table(&err->table, 3);

        init_buffer(&bf);
        writeu_buffer(bf, exact);
        minim_error_desc_table_set(err->table, 0, "expected", get_buffer(bf));
    }
    else if (MINIM_OBJ_PAIRP(args[1]))
    {
        debug_print_minim_object(args[1], NULL);

        if (!minim_exact_nonneg_intp(MINIM_CAR(args[1])))
            THROW(env, minim_argument_error("arity?", "arity-error", 1, args[1]));

        if (!minim_exact_nonneg_intp(MINIM_CDR(args[1])) && MINIM_CDR(args[1]) != minim_false)
            THROW(env, minim_argument_error("arity?", "arity-error", 1, args[1]));

        init_minim_error(&err, "arity error", MINIM_SYMBOL(args[0]));
        init_minim_error_desc_table(&err->table, 3);

        init_buffer(&bf);
        if (MINIM_CDR(args[1]) == minim_false)      // minimum only
        {
            writef_buffer(bf, "at least ~u", MINIM_NUMBER_TO_UINT(MINIM_CAR(args[1])));
        }
        else    // range
        {
            writef_buffer(bf, "between ~u and ~u",
                              MINIM_NUMBER_TO_UINT(MINIM_CAR(args[1])),
                              MINIM_NUMBER_TO_UINT(MINIM_CDR(args[1])));
        }

        minim_error_desc_table_set(err->table, 0, "expected", get_buffer(bf));
    }
    else
    {
        THROW(env, minim_argument_error("arity?", "arity-error", 1, args[1]));
    }

    clear_buffer(bf);
    writeu_buffer(bf, argc - 2);
    minim_error_desc_table_set(err->table, 1, "given", get_buffer(bf));

    clear_buffer(bf);
    set_default_print_params(&pp);
    for (size_t i = 2; i < argc; ++i)
    {
        writef_buffer(bf, "\n;    ");
        print_to_buffer(bf, args[i], env, &pp);
    }
    
    minim_error_desc_table_set(err->table, 2, "arguments", get_buffer(bf));
    THROW(env, minim_err(err));
    return minim_null;
}

MinimObject *minim_builtin_syntax_error(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SYMBOLP(args[0]) && !minim_falsep(args[0]))
        THROW(env, minim_argument_error("string? or #f", "syntax-error", 0, args[0]));

    if (!MINIM_OBJ_SYMBOLP(args[1]) && !MINIM_OBJ_STRINGP(args[1]))
        THROW(env, minim_argument_error("symbol?/string?", "syntax-error", 1, args[1]));

    if (argc == 2)
    {
        THROW(env, minim_syntax_error(MINIM_STRING(args[1]),
                                      (minim_falsep(args[0]) ? NULL : MINIM_STRING(args[0])),
                                      NULL,
                                      NULL));
    }
    else if (argc == 3)
    {
        if (!MINIM_OBJ_ASTP(args[2]))
            THROW(env, minim_argument_error("syntax?", "syntax-error", 2, args[2]));

        THROW(env, minim_syntax_error(MINIM_STRING(args[1]),
                                      (minim_falsep(args[0]) ? NULL : MINIM_STRING(args[0])),
                                      MINIM_AST_VAL(args[2]),
                                      NULL));
    }
    else
    {
        if (!MINIM_OBJ_ASTP(args[3]))
            THROW(env, minim_argument_error("syntax?", "syntax-error", 3, args[3]));

        THROW(env, minim_syntax_error(MINIM_STRING(args[1]),
                                      (minim_falsep(args[0]) ? NULL : MINIM_STRING(args[0])),
                                      MINIM_AST_VAL(args[2]),
                                      MINIM_AST_VAL(args[3])));
    }

    return NULL; // avoid compile errors
}

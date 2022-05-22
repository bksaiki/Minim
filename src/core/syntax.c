#include "minimpriv.h"

// =============================== Syntax Location ==========================

SyntaxLoc *init_syntax_loc(MinimObject *src,
                           size_t row,
                           size_t col,
                           size_t pos,
                           size_t span)
{
    SyntaxLoc *loc = GC_alloc(sizeof(SyntaxLoc));
    loc->src = src;
    loc->row = row;
    loc->col = col;
    loc->pos = pos;
    loc->span = span;
    return loc;
}

size_t syntax_list_len(MinimObject *stx)
{
    size_t len = 0;

    if (MINIM_STX_PAIRP(stx))
        stx = MINIM_STX_VAL(stx);

    while (MINIM_OBJ_PAIRP(stx))
    {
        ++len;
        stx = MINIM_CDR(stx);
        if (MINIM_STX_PAIRP(stx))
            stx = MINIM_STX_VAL(stx);
    }
    
    return len;
}

size_t syntax_proper_list_len(MinimObject *stx)
{
    size_t len = 0;

    stx = MINIM_STX_VAL(stx);
    while (MINIM_OBJ_PAIRP(stx))
    {
        ++len;
        stx = MINIM_CDR(stx);
        if (MINIM_STX_PAIRP(stx))
            stx = MINIM_STX_VAL(stx);
    }
    
    return (minim_nullp(stx) ? len : SIZE_MAX);
}

// ================================ Checker ================================

#define CHECK_REC(proc, x, expr)        if (proc == x) expr(env, stx);

static void check_syntax_rec(MinimEnv *env, MinimObject *stx);

static void check_syntax_set(MinimEnv *env, MinimObject *stx)
{
    MinimObject *s, *sym;
    
    s = MINIM_STX_CDR(stx);
    sym = MINIM_STX_VAL(MINIM_CAR(s));
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        THROW(env, minim_syntax_error("not an identifier",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      MINIM_CAR(s)));
    }

    env_intern_sym(env, MINIM_STRING(sym), minim_void);
    check_syntax_rec(env, MINIM_CADR(s));
}

static void check_syntax_def_values(MinimEnv *env, MinimObject *stx)
{
    MinimObject *ids, *vals, *it;
    size_t idc;

    it = MINIM_STX_CDR(stx);
    ids = MINIM_STX_VAL(MINIM_CAR(it));
    vals = MINIM_CADR(it);
    if (!minim_listp(ids))
    {
        THROW(env, minim_syntax_error("not a list of identifiers",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx, ids));
    }

    it = ids;
    idc = syntax_list_len(ids);
    for (size_t i = 0; i < idc; ++i)
    {
        MinimObject *sym = MINIM_STX_VAL(MINIM_CAR(it));
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            THROW(env, minim_syntax_error("not an identifier",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                          stx, sym));
        }

        if (env_get_local_sym(env, MINIM_SYMBOL(sym)) != NULL)
        {
            THROW(env, minim_syntax_error("duplicate identifier",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                          stx, sym));
        }
        
        env_intern_sym(env, MINIM_STRING(sym), minim_void);
        it = MINIM_CDR(it);
    }

    check_syntax_rec(env, vals);
}

static void check_syntax_lambda(MinimEnv *env, MinimObject *stx)
{
    MinimObject *lst, *args, *sym;
    MinimEnv *env2;

    env2 = init_env(env);
    lst = MINIM_STX_VAL(stx);
    args = MINIM_CADR(lst);
    if (MINIM_STX_PAIRP(args))
    {
        for (MinimObject *it = MINIM_STX_VAL(args); it && !minim_nullp(it); it = MINIM_CDR(it))
        {
            if (minim_nullp(MINIM_CDR(it)) || MINIM_OBJ_PAIRP(MINIM_CDR(it)))
            {
                sym = MINIM_STX_VAL(MINIM_CAR(it));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  MINIM_STX_VAL(MINIM_CAR(it))));
                }

                env_intern_sym(env2, MINIM_STRING(sym), minim_void);
            }
            else // ... arg_n . arg_rest)
            {
                sym = MINIM_STX_VAL(MINIM_CAR(it));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  MINIM_STX_VAL(MINIM_CAR(it))));
                }
                else
                {
                    env_intern_sym(env2, MINIM_STRING(sym), minim_void); 
                }

                sym = MINIM_STX_VAL(MINIM_CDR(it));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  MINIM_STX_VAL(MINIM_CDR(it))));
                }

                env_intern_sym(env2, MINIM_STRING(sym), minim_void);
                break;
            }
        }
    }
    else if (!MINIM_STX_SYMBOLP(args) && !MINIM_STX_NULLP(args))
    {
        THROW(env, minim_syntax_error("expected argument names",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      args));
    }
    
    for (lst = MINIM_CDDR(lst); !minim_nullp(lst); lst = MINIM_CDR(lst))
        check_syntax_rec(env2, MINIM_CAR(lst));
}

static void check_syntax_begin(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
        check_syntax_rec(env, MINIM_CAR(it));
}

static void check_syntax_let_values(MinimEnv *env, MinimObject *stx)
{
    MinimObject *body, *bindings;
    MinimEnv *env2;
    
    env2 = init_env(env);
    body = MINIM_STX_CDR(stx);
    bindings = MINIM_CAR(body);
    body = MINIM_CDR(body);

    if (minim_nullp(body))
    {
        THROW(env, minim_syntax_error("missing body",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));
    }

    // early exit: (let () ...)
    if (!MINIM_STX_NULLP(bindings))
    {
        for (MinimObject *it = MINIM_STX_VAL(bindings); !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *bind, *ids;

            if (!MINIM_OBJ_PAIRP(it))
            {
                THROW(env, minim_syntax_error("expected a list of bindings",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                              stx,
                                              bindings));
            }

            bind = MINIM_CAR(it);
            if (!MINIM_STX_PAIRP(bind) || syntax_list_len(bind) != 2)
            {
                THROW(env, minim_syntax_error("bad binding",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                              stx,
                                              bind));
            }

            // list of identifier
            bind = MINIM_STX_VAL(bind);
            ids = MINIM_CAR(bind);
            for (MinimObject *it2 = MINIM_STX_VAL(ids); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *sym;

                if (!MINIM_OBJ_PAIRP(it2))
                {
                    THROW(env, minim_syntax_error("not a list of identifiers",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  ids));
                }

                sym = MINIM_CAR(it2);
                if (!MINIM_STX_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  sym));
                }

                if (env_get_local_sym(env2, MINIM_STX_SYMBOL(sym)) != NULL)
                {
                    THROW(env, minim_syntax_error("duplicate identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                                  stx,
                                                  sym));
                }

                env_intern_sym(env2, MINIM_STX_SYMBOL(sym), minim_void);
            }

            // this is incorrect, behaves more like a letrec
            // should take env rather than env2
            check_syntax_rec(env2, MINIM_CADR(bind));
        }
    }

    for (; !minim_nullp(body); body = MINIM_CDR(body))
        check_syntax_rec(env2, MINIM_CAR(body));
}

static void check_syntax_1arg(MinimEnv *env, MinimObject *stx)
{
    check_syntax_rec(env, MINIM_CADR(MINIM_STX_VAL(stx)));
}

static void check_syntax_syntax_case(MinimEnv *env, MinimObject *stx)
{
    MinimObject *datum, *reserved, *rules, *it;

    rules = MINIM_STX_CDR(stx);
    datum = MINIM_CAR(rules);
    rules = MINIM_CDR(rules);
    reserved = MINIM_CAR(rules);
    rules = MINIM_CDR(rules);

    check_syntax_rec(env, datum);
    it = MINIM_STX_VAL(reserved);
    for (; MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
    {
        if (!MINIM_STX_SYMBOLP(MINIM_CAR(it)))
        {
            THROW(env, minim_syntax_error("expected a list of symbols",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                          stx,
                                          MINIM_CAR(it)));
        }
    }

    if (!minim_nullp(it))
    {
        THROW(env, minim_syntax_error("expected a list of bindings",
                                        MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                        stx,
                                        reserved));
    }

    for (it = rules; MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
    {
        MinimObject *match, *replace;
        replace = MINIM_STX_VAL(MINIM_CAR(it));
        match = MINIM_CAR(replace);
        replace = MINIM_CADR(replace);
        check_transform(env, match, replace, reserved);
    }

    if (!minim_nullp(it))
    {
        THROW(env, minim_syntax_error("expected a transform rule",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));       
    }
}

static void check_syntax_import(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *sym = MINIM_STX_VAL(MINIM_CAR(it));
        if (!MINIM_OBJ_SYMBOLP(sym) && !MINIM_OBJ_STRINGP(sym))
        {
            THROW(env, minim_syntax_error("import must be a symbol or string",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                          stx,
                                          MINIM_CAR(it)));
        }
    }
}

static MinimObject *invalid_export_error(MinimObject *stx, MinimObject *export_lst)
{
    return minim_syntax_error("not a valid export form",
                              MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                              stx,
                              MINIM_CAR(export_lst));

}

static void check_syntax_export(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *export;
        
        export = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_listp(export))
        {
            MinimObject *attrib;

            if (minim_list_length(export) != 2)
                THROW(env, invalid_export_error(stx, it));

            attrib = MINIM_STX_VAL(MINIM_CAR(export));
            if (!MINIM_OBJ_SYMBOLP(attrib) || strcmp(MINIM_SYMBOL(attrib), "all") != 0)
                THROW(env, invalid_export_error(stx, it));
            export = MINIM_STX_VAL(MINIM_CADR(export));
        }
        
        if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            THROW(env, invalid_export_error(stx, it));
    }
}

static void check_syntax_module(MinimEnv *env, MinimObject *stx)
{
    MinimObject *li, *datum;
    
    li = MINIM_STX_CDR(stx);
    if (minim_nullp(li))
    {
        THROW(env, minim_syntax_error("not a valid module form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));
    }

    datum = MINIM_CAR(li);
    if (!MINIM_STX_SYMBOLP(datum) && !minim_eqvp(MINIM_STX_VAL(datum), minim_false))
    {
        THROW(env, minim_syntax_error("expected a symbol or #f",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      datum));
    }

    li = MINIM_CDR(li);
    if (minim_nullp(li))
    {
        THROW(env, minim_syntax_error("not a valid module form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));
    }

    datum = MINIM_CAR(li);
    if (!MINIM_STX_SYMBOLP(datum) &&
        !MINIM_OBJ_STRINGP(MINIM_STX_VAL(datum)) &&
        !minim_eqvp(MINIM_STX_VAL(datum), minim_false))
    {
        THROW(env, minim_syntax_error("expected a string, symbol, or #f",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      datum));
    }

    li = MINIM_CDR(li);
    if (minim_nullp(li))
    {
        THROW(env, minim_syntax_error("not a valid module form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));
    }

    datum = MINIM_CAR(li);
    if (!minim_nullp(MINIM_CDR(li)))
    {
        THROW(env, minim_syntax_error("not a valid module form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      NULL));
    }

    li = datum;
    if (minim_nullp(li))
    {
        THROW(env, minim_syntax_error("expected a %%module-begin form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      li));
    }

    datum = MINIM_STX_CAR(li);
    if (!MINIM_STX_SYMBOLP(datum) && minim_eqvp(MINIM_STX_VAL(datum), intern("%module-begin")))
    {
        THROW(env, minim_syntax_error("expected a %%module-begin form",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                      stx,
                                      li));
    }

    for (MinimObject *it = MINIM_STX_CDR(li); !minim_nullp(it); it = MINIM_CDR(it))
        check_syntax_rec(env, MINIM_CAR(it));
}

static void check_syntax_rec(MinimEnv *env, MinimObject *stx)
{
    if (!MINIM_STX_PAIRP(stx))      // early exit, no checking
        return;

    if (MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
    {
        MinimObject *head, *op;
        
        // special case: %module
        head =  MINIM_STX_CAR(stx);
        if (minim_eqvp(MINIM_STX_VAL(head), intern("%module")))
        {
            check_syntax_module(env, stx);
            return;
        }
        else if (minim_eqvp(MINIM_STX_VAL(head), intern("%top")))
        {
            if (!MINIM_STX_SYMBOLP(MINIM_STX_CDR(stx)))
            {
                THROW(env, minim_syntax_error("variable reference must be a symbol",
                                              MINIM_STX_SYMBOL(head),
                                              stx,
                                              NULL));
            }

            return;
        }

        op = env_get_sym(env, MINIM_STX_SYMBOL(head));
        if (!op || MINIM_OBJ_TRANSFORMP(op))
        {
            return;
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimPrimClosureFn proc = MINIM_SYNTAX(op);

            if (!minim_check_syntax_arity(proc, minim_list_length(MINIM_STX_CDR(stx)), env))
            {
                THROW(env, minim_syntax_error("bad syntax",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)),
                                              stx, NULL));
            }

            CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_setb, check_syntax_set);
            CHECK_REC(proc, minim_builtin_def_values, check_syntax_def_values);
            CHECK_REC(proc, minim_builtin_if, check_syntax_begin);                  // same as begin
            CHECK_REC(proc, minim_builtin_let_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_lambda, check_syntax_lambda);
            CHECK_REC(proc, minim_builtin_delay, check_syntax_1arg);
            CHECK_REC(proc, minim_builtin_callcc, check_syntax_1arg)

            CHECK_REC(proc, minim_builtin_def_syntaxes, check_syntax_def_values);   // same as def_values
            CHECK_REC(proc, minim_builtin_syntax_case, check_syntax_syntax_case);

            CHECK_REC(proc, minim_builtin_import, check_syntax_import);
            CHECK_REC(proc, minim_builtin_export, check_syntax_export);

            // minim_builtin_quote
            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template
        }
        else if (MINIM_OBJ_FUNCP(op))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                check_syntax_rec(env, MINIM_CAR(it));
        }
    }
    else
    {
        for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
            check_syntax_rec(env, MINIM_CAR(it));
    }
}

// ================================ Syntax Conversions ================================

MinimObject *datum_to_syntax(MinimEnv *env, MinimObject *obj)
{
    Buffer *bf;

    if (minim_specialp(obj))
    {
        return minim_ast(obj, NULL);
    }
    else if (MINIM_OBJ_ASTP(obj))
    {
        return obj;
    }
    else if (MINIM_OBJ_STRINGP(obj) ||
             MINIM_OBJ_SYMBOLP(obj) ||
             MINIM_OBJ_NUMBERP(obj))
    {
        return minim_ast(obj, NULL);
    }
    else if (MINIM_OBJ_PAIRP(obj))
    {
        MinimObject *it, *trailing;
        trailing = NULL;
        for (it = obj; MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            MINIM_CAR(it) = datum_to_syntax(env, MINIM_CAR(it));
            trailing = it;
        }

        if (trailing && !minim_nullp(MINIM_CDR(trailing)))   // not a list
        {
            MinimObject *cdr = datum_to_syntax(env, MINIM_CDR(trailing));
            MINIM_CDR(trailing) = (trailing == obj) ? MINIM_STX_VAL(cdr) : cdr;
        }
        
        return minim_ast(obj, NULL);
    }
    else if (MINIM_OBJ_VECTORP(obj))
    {
        for (size_t i = 0; i < MINIM_VECTOR_LEN(obj); ++i)
            MINIM_VECTOR_REF(obj, i) = datum_to_syntax(env, MINIM_VECTOR_REF(obj, i));

        return minim_ast(obj, NULL);
    }
    else
    {
        PrintParams pp;
        MinimObject *err;

        set_default_print_params(&pp);
        init_buffer(&bf);
        print_to_buffer(bf, obj, NULL, &pp);

        err = minim_error("can't convert to syntax", NULL);
        init_minim_error_desc_table(&MINIM_ERROR(err)->table, 1);
        minim_error_desc_table_set(MINIM_ERROR(err)->table, 0, "at", get_buffer(bf));
        THROW(env, err);
    }
}

static MinimObject *syntax_unwrap_node(MinimEnv *env, MinimObject* stx, bool unquote)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *res, *it, *it2, *trailing;
        
        it = MINIM_STX_CAR(stx);
        if (unquote && MINIM_STX_SYMBOLP(it)) // unquote
        {
            MinimObject *proc = env_get_sym(env, MINIM_STX_SYMBOL(it));
            if (proc && (MINIM_SYNTAX(proc) == minim_builtin_unquote))
                return eval_ast_no_check(env, MINIM_STX_CADR(stx));
        }

        it2 = NULL;
        trailing = NULL;
        res = minim_null;
        for (it = MINIM_STX_VAL(stx); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            MinimObject *val = syntax_unwrap_node(env, MINIM_CAR(it), unquote);
            if (minim_nullp(res))
            {
                res = minim_cons(val, minim_null);
                it2 = res;
            }
            else
            {
                MINIM_CDR(it2) = minim_cons(val, minim_null);
                it2 = MINIM_CDR(it2);
            }

            trailing = it;
        }

        if (trailing && !minim_nullp(it))   // not a list
            MINIM_CDR(it2) = syntax_unwrap_node(env, it, unquote);

        return res;
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject *res, *obj;
        size_t len;

        obj = MINIM_STX_VAL(stx);
        len = MINIM_VECTOR_LEN(obj);
        res = minim_vector(len, GC_alloc(len * sizeof(MinimObject*)));
        for (size_t i = 0; i < len; ++i)
            MINIM_VECTOR_REF(res, i) = syntax_unwrap_node(env, MINIM_VECTOR_REF(obj, i), unquote);

        return res;
    }

    return MINIM_STX_VAL(stx);
}

// ================================ Public ================================

void check_syntax(MinimEnv *env, MinimObject *stx)
{
    check_syntax_rec(init_env(env), stx);
}

MinimObject *syntax_unwrap_rec(MinimEnv *env, MinimObject *stx)
{
    return syntax_unwrap_node(env, stx, false);
}

MinimObject *syntax_unwrap_rec2(MinimEnv *env, MinimObject *stx)
{
    return syntax_unwrap_node(env, stx, true);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    return args[0];
}

MinimObject *minim_builtin_syntaxp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_ASTP(args[0]));
}

MinimObject *minim_builtin_unwrap(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_ASTP(args[0]))
        THROW(env, minim_argument_error("syntax?", "unwrap", 0, args[0]));
    
    return MINIM_STX_VAL(args[0]);
}

MinimObject *minim_builtin_to_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    return datum_to_syntax(env, args[0]);
}

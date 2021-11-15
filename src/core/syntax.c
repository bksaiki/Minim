#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "global.h"
#include "list.h"
#include "hash.h"
#include "syntax.h"
#include "transform.h"

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

    if (MINIM_STX_PAIRP(stx))
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

#define CHECK_REC(proc, x, expr)        if (proc == x) expr(env, ast);

static void check_syntax_rec(MinimEnv *env, MinimObject *ast);
static MinimObject *constant_fold_rec(MinimEnv *env, MinimObject *ast);

static void check_syntax_set(MinimEnv *env, MinimObject *ast)
{
    MinimObject *s, *sym;
    
    s = MINIM_STX_CDR(ast);
    sym = MINIM_STX_VAL(MINIM_CAR(s));
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        THROW(env, minim_syntax_error("not an identifier",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast,
                                      MINIM_CAR(s)));
    }

    env_intern_sym(env, MINIM_STRING(sym), minim_void);
    check_syntax_rec(env, MINIM_CADR(s));
}

static void check_syntax_def_values(MinimEnv *env, MinimObject *ast)
{
    MinimObject *ids, *vals, *it;
    size_t idc;

    it = MINIM_STX_CDR(ast);
    ids = MINIM_STX_VAL(MINIM_CAR(it));
    vals = MINIM_CADR(it);
    if (!minim_listp(ids))
    {
        THROW(env, minim_syntax_error("not a list of identifiers",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast, ids));
    }

    it = ids;
    idc = syntax_list_len(ids);
    for (size_t i = 0; i < idc; ++i)
    {
        MinimObject *sym;
        size_t hash;
        char *s;
        
        sym = MINIM_STX_VAL(MINIM_CAR(it));
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            THROW(env, minim_syntax_error("not an identifier",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                          ast, sym));
        }

        s = MINIM_SYMBOL(sym);
        hash = hash_bytes(s, strlen(s));
        if (minim_symbol_table_get(env->table, s, hash) != NULL)
        {
            THROW(env, minim_syntax_error("duplicate identifier",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                          ast, sym));
        }
        
        env_intern_sym(env, MINIM_STRING(sym), minim_void);
        it = MINIM_CDR(it);
    }

    check_syntax_rec(env, vals);
}

static void check_syntax_lambda(MinimEnv *env, MinimObject *ast)
{
    MinimObject *lst, *args, *sym;
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    lst = MINIM_STX_VAL(ast);
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
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
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
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
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
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
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
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast,
                                      args));
    }
    
    for (lst = MINIM_CDDR(lst); !minim_nullp(lst); lst = MINIM_CDR(lst))
        check_syntax_rec(env2, MINIM_CAR(lst));
}

static void check_syntax_begin(MinimEnv *env, MinimObject *ast)
{
    for (MinimObject *it = MINIM_STX_VAL(ast); !minim_nullp(it); it = MINIM_CDR(it))
        check_syntax_rec(env, MINIM_CAR(it));
}

static void check_syntax_let_values(MinimEnv *env, MinimObject *ast)
{
    MinimObject *body, *bindings;
    MinimEnv *env2;
    
    init_env(&env2, env, NULL);
    body = MINIM_STX_CDR(ast);
    bindings = MINIM_CAR(body);
    body = MINIM_CDR(body);

    if (minim_nullp(body))
    {
        THROW(env, minim_syntax_error("missing body",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast,
                                      NULL));
    }

    // early exit: (let () ...)
    if (!minim_nullp(MINIM_STX_VAL(bindings)))
    {
        for (MinimObject *it = MINIM_STX_VAL(bindings); !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *bind, *ids;

            if (!MINIM_OBJ_PAIRP(it))
            {
                THROW(env, minim_syntax_error("expected a list of bindings",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                              ast,
                                              bindings));
            }

            bind = MINIM_CAR(it);
            if (!MINIM_STX_PAIRP(bind) || syntax_list_len(bind) != 2)
            {
                THROW(env, minim_syntax_error("bad binding",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                              ast,
                                              bind));
            }

            // list of identifier
            bind = MINIM_STX_VAL(bind);
            ids = MINIM_CAR(bind);
            for (MinimObject *it2 = MINIM_STX_VAL(ids); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *sym;
                size_t hash;
                char *s;

                if (!MINIM_OBJ_PAIRP(it2))
                {
                    THROW(env, minim_syntax_error("not a list of identifiers",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  ids));
                }

                sym = MINIM_CAR(it2);
                if (!MINIM_STX_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  sym));
                }

                s = MINIM_STX_SYMBOL(sym);
                hash = hash_bytes(s, strlen(s));
                if (minim_symbol_table_get(env2->table, s, hash) != NULL)
                {
                    THROW(env, minim_syntax_error("duplicate identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  sym));
                }

                env_intern_sym(env2, s, minim_void);
            }

            // this is incorrect, behaves more like a letrec
            // should take env rather than env2
            check_syntax_rec(env2, MINIM_CADR(bind));
        }
    }

    for (; !minim_nullp(body); body = MINIM_CDR(body))
        check_syntax_rec(env2, MINIM_CAR(body));
}

static void check_syntax_letstar_values(MinimEnv *env, MinimObject *ast)
{
        MinimObject *body, *bindings;
    MinimEnv *env2;
    
    init_env(&env2, env, NULL);
    body = MINIM_STX_CDR(ast);
    bindings = MINIM_CAR(body);
    body = MINIM_CDR(body);

    if (minim_nullp(body))
    {
        THROW(env, minim_syntax_error("missing body",
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast,
                                      NULL));
    }

    // early exit: (let () ...)
    if (!minim_nullp(MINIM_STX_VAL(bindings)))
    {
        for (MinimObject *it = MINIM_STX_VAL(bindings); !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *bind, *ids;
            MinimEnv *env3;

            if (!MINIM_OBJ_PAIRP(it))
            {
                THROW(env, minim_syntax_error("expected a list of bindings",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                              ast,
                                              bindings));
            }

            bind = MINIM_CAR(it);
            if (!MINIM_STX_PAIRP(bind) || syntax_list_len(bind) != 2)
            {
                THROW(env, minim_syntax_error("bad binding",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                              ast,
                                              bind));
            }

            // list of identifier
            bind = MINIM_STX_VAL(bind);
            ids = MINIM_CAR(bind);
            init_env(&env3, env2, NULL);
            for (MinimObject *it2 = MINIM_STX_VAL(ids); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *sym;
                size_t hash;
                char *s;

                if (!MINIM_OBJ_PAIRP(it2))
                {
                    THROW(env, minim_syntax_error("not a list of identifiers",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  ids));
                }

                sym = MINIM_CAR(it2);
                if (!MINIM_STX_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  sym));
                }

                s = MINIM_STX_SYMBOL(sym);
                hash = hash_bytes(s, strlen(s));
                if (minim_symbol_table_get(env3->table, s, hash) != NULL)
                {
                    THROW(env, minim_syntax_error("duplicate identifier",
                                                  MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                                  ast,
                                                  sym));
                }

                env_intern_sym(env3, s, minim_void);
            }

            // this is incorrect, behaves more like a letrec
            // should take env2 rather than env3
            check_syntax_rec(env3, MINIM_CADR(bind));
            minim_symbol_table_merge(env2->table, env3->table);
        }
    }

    for (; !minim_nullp(body); body = MINIM_CDR(body))
        check_syntax_rec(env2, MINIM_CAR(body));
}

static void check_syntax_1arg(MinimEnv *env, MinimObject *ast)
{
    check_syntax_rec(env, MINIM_CADR(MINIM_STX_VAL(ast)));
}

static void check_syntax_syntax_case(MinimEnv *env, MinimObject *ast)
{
    MinimObject *datum, *reserved, *rules, *it;

    rules = MINIM_STX_CDR(ast);
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
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                          ast,
                                          MINIM_CAR(it)));
        }
    }

    if (!minim_nullp(it))
    {
        THROW(env, minim_syntax_error("expected a list of bindings",
                                        MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                        ast,
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
                                      MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                      ast,
                                      NULL));       
    }
}

static void check_syntax_import(MinimEnv *env, MinimObject *ast)
{
    for (MinimObject *it = MINIM_STX_VAL(ast); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *sym = MINIM_STX_VAL(MINIM_CAR(it));
        if (!MINIM_OBJ_SYMBOLP(sym) && !MINIM_OBJ_STRINGP(sym))
        {
            THROW(env, minim_syntax_error("import must be a symbol or string",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                          ast,
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

static void check_syntax_export(MinimEnv *env, MinimObject *ast)
{
    for (MinimObject *it = MINIM_STX_VAL(ast); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *export;
        
        export = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_listp(export))
        {
            MinimObject *attrib;

            if (syntax_proper_list_len(export) != 2)
                THROW(env, invalid_export_error(ast, it));

            attrib = MINIM_STX_VAL(MINIM_CAR(export));
            if (!MINIM_OBJ_SYMBOLP(attrib) || strcmp(MINIM_STRING(attrib), "all") != 0)
                THROW(env, invalid_export_error(ast, it));

            export = MINIM_STX_VAL(MINIM_CADR(export));
        }
        
        if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            THROW(env, invalid_export_error(ast, it));
    }
}

static void check_syntax_rec(MinimEnv *env, MinimObject *ast)
{
    if (!MINIM_STX_PAIRP(ast))      // early exit, no checking
        return;

    if (MINIM_STX_SYMBOLP(MINIM_STX_CAR(ast)))
    {
        MinimObject *op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)));
        if (!op)
        {
            THROW(env, minim_syntax_error("unknown identifier",
                                          MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                          ast, NULL));
        }

        if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = MINIM_SYNTAX(op);

            if (!minim_check_syntax_arity(proc, minim_list_length(MINIM_STX_CDR(ast)), env))
            {
                THROW(env, minim_syntax_error("bad syntax",
                                              MINIM_STX_SYMBOL(MINIM_STX_CAR(ast)),
                                              ast, NULL));
            }

            CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_setb, check_syntax_set);
            CHECK_REC(proc, minim_builtin_def_values, check_syntax_def_values);
            CHECK_REC(proc, minim_builtin_if, check_syntax_begin);                  // same as begin
            CHECK_REC(proc, minim_builtin_let_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_letstar_values, check_syntax_letstar_values);
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
            for (MinimObject *it = MINIM_STX_CDR(ast); !minim_nullp(it); it = MINIM_CDR(it))
                check_syntax_rec(env, MINIM_CAR(it));
        }
    }
    else
    {
        for (MinimObject *it = MINIM_STX_VAL(ast); !minim_nullp(it); it = MINIM_CDR(it))
            check_syntax_rec(env, MINIM_CAR(it));
    }
}

// ================================ Constant Folder ================================

#define FOLD_REC(x, expr)   if (proc == x) return expr(env, stx);

static MinimObject *fold_datum(MinimEnv *env, MinimObject *stx, MinimObject *obj)
{
    if (minim_nullp(obj) || minim_voidp(obj))
    {
        return minim_cons(stx, obj);
    }
    else if (minim_specialp(obj))
    {
        return minim_cons(datum_to_syntax(env, obj), obj);
    }
    else if (MINIM_OBJ_FUNCP(obj) || MINIM_OBJ_VALUESP(obj))
    {
        return minim_cons(stx, NULL);
    }
    else if (MINIM_OBJ_STRINGP(obj))
    {
        return minim_cons(datum_to_syntax(env, obj), obj);
    }
    else
    {
        return minim_cons(stx, obj);
    }
}

static MinimObject *fold_syntax_def_values(MinimEnv *env, MinimObject *stx)
{
    MinimObject *t = minim_list_drop(MINIM_STX_VAL(stx), 2);
    MINIM_CAR(t) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(t)));
    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_let_values(MinimEnv *env, MinimObject *stx)
{
    MinimObject *bindings, *body;
    
    body = MINIM_STX_CDR(stx);
    bindings = MINIM_CAR(body);
    body = MINIM_CDR(body);

    // ignore if: (let-values () ...)
    if (!minim_nullp(MINIM_STX_VAL(bindings)))
    {
        for (MinimObject *it = MINIM_STX_VAL(bindings); !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *bind = MINIM_STX_CDR(MINIM_CAR(it));
            MINIM_CAR(bind) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(bind)));
        }
    }

    for (; !minim_nullp(body); body = MINIM_CDR(body))
        MINIM_CAR(body) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(body)));
    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_lambda(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = minim_list_drop(MINIM_STX_VAL(stx), 2); !minim_nullp(it); it = MINIM_CDR(it))
        MINIM_CAR(it) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(it)));

    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_begin(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
        MINIM_CAR(it) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(it)));

    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_if(MinimEnv *env, MinimObject *stx)
{
    MinimObject *t = MINIM_STX_CDR(stx);

    MINIM_CAR(t) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(t)));     // cond
    t = MINIM_CDR(t);
    MINIM_CAR(t) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(t)));     // ift
    t = MINIM_CDR(t);
    MINIM_CAR(t) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(t)));     // iff

    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_1arg(MinimEnv *env, MinimObject *stx)
{
    MinimObject *t = MINIM_STX_CDR(stx);
    MINIM_CAR(t) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(t)));
    return minim_cons(stx, NULL);
}

static MinimObject *fold_syntax_quote(MinimEnv *env, MinimObject *stx)
{
    MinimObject *obj = unsyntax_ast_rec(env, MINIM_STX_CADR(stx));
    return fold_datum(env, stx, obj);
}

static MinimObject *constant_fold_rec(MinimEnv *env, MinimObject *stx)
{
    MinimObject *op;

    if (!MINIM_STX_PAIRP(stx))
    {
        MinimObject *obj = eval_ast_terminal(env, stx);
        return (obj == NULL) ? minim_cons(stx, NULL) : fold_datum(env, stx, obj);
    }

    op = MINIM_STX_CAR(stx);
    if (MINIM_STX_SYMBOLP(op))
    {
        MinimBuiltin proc;
        
        op = env_get_sym(env, MINIM_STX_SYMBOL(op));
        if (!op || MINIM_OBJ_CLOSUREP(op))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                MINIM_CAR(it) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(it)));

            return minim_cons(stx, NULL);
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            proc = MINIM_SYNTAX(op);

            FOLD_REC(minim_builtin_setb, fold_syntax_def_values);
            FOLD_REC(minim_builtin_def_values, fold_syntax_def_values);
            FOLD_REC(minim_builtin_let_values, fold_syntax_let_values);
            FOLD_REC(minim_builtin_letstar_values, fold_syntax_let_values);
            FOLD_REC(minim_builtin_def_syntaxes, fold_syntax_def_values);
            FOLD_REC(minim_builtin_lambda, fold_syntax_lambda);
            
            FOLD_REC(minim_builtin_begin, fold_syntax_begin);
            FOLD_REC(minim_builtin_if, fold_syntax_if);
            FOLD_REC(minim_builtin_delay, fold_syntax_1arg);
            FOLD_REC(minim_builtin_callcc, fold_syntax_1arg);
            FOLD_REC(minim_builtin_quote, fold_syntax_quote);

            // minim_builtin_import
            // minim_builtin_export
            // minim_building_syntax_case
            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template

            return minim_cons(stx, NULL);
        }
        else if (MINIM_OBJ_BUILTINP(op))
        {
            MinimObject **args;
            size_t argc, i;
            bool foldp;

            foldp = true;
            argc = minim_list_length(MINIM_STX_CDR(stx));
            proc = MINIM_BUILTIN(op);

            i = 0;
            args = GC_alloc(argc * sizeof(MinimObject*));
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
            {
                MinimObject *arg;

                arg = constant_fold_rec(env, MINIM_CAR(it));
                MINIM_CAR(it) = MINIM_CAR(arg);

                if (!MINIM_CDR(arg))    foldp = false;
                else if (foldp)         args[i] = MINIM_CDR(arg);
                ++i;
            }
            
            if (foldp && builtin_foldablep(proc))
            {
                return fold_datum(env, stx, proc(env, argc, args));
            }
            else
            {
                return minim_cons(stx, NULL);
            }
        }
        else
        {
            return minim_cons(stx, NULL);
        }
    }
    else
    {
        for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
            MINIM_CAR(it) = MINIM_CAR(constant_fold_rec(env, MINIM_CAR(it)));

        return minim_cons(stx, NULL);
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

// ================================ Public ================================

void check_syntax(MinimEnv *env, MinimObject *ast)
{
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    check_syntax_rec(env2, ast);
}

MinimObject *constant_fold(MinimEnv *env, MinimObject *stx)
{
    // printf("> "); print_syntax_to_port(stx, stdout); printf("\n");
    stx = ((MinimObject*) MINIM_CAR(constant_fold_rec(env, stx)));
    // printf("< "); print_syntax_to_port(stx, stdout); printf("\n");
    return stx;
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

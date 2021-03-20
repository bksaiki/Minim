#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/read.h"
#include "assert.h"
#include "ast.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "number.h"

MinimObject *minim_builtin_if(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_ERRORP(cond))
    {
        eval_ast(env, coerce_into_bool(cond) ?
                      args[1]->u.ptrs.p1 :
                      args[2]->u.ptrs.p1, &res);
        free_minim_object(cond);
    }
    else
        res = cond;

    return res;
}

MinimObject *minim_builtin_cond(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool eval = false;

    for (size_t i = 0; !eval && i < argc; ++i)
    {
        MinimObject *ce_pair, *cond, *val;
        MinimAst *cond_syn;

        unsyntax_ast(env, args[i]->u.ptrs.p1, &ce_pair);
        if (!minim_listp(ce_pair) || minim_list_length(ce_pair) < 2)
        {
            res = minim_argument_error("([<cond> <exprs> ...] ...)", "cond", SIZE_MAX, ce_pair);
            free_minim_object(ce_pair);
            return res;
        }

        cond_syn = MINIM_CAR(ce_pair)->u.ptrs.p1;
        if (i + 1 != argc && cond_syn->sym && strcmp(cond_syn->sym, "else") == 0)
        {   
            free_minim_object(ce_pair);
            minim_error(&res, "else clause must be last");
            return res;
        }

        eval_ast(env, cond_syn, &cond);
        if (coerce_into_bool(cond))
        {
            eval = true;
            if (minim_list_length(ce_pair) > 2)
            {
                MinimEnv *env2;

                init_env(&env2, env);
                for (MinimObject *it = MINIM_CDR(ce_pair); it; it = MINIM_CDR(it))
                {
                    eval_ast(env2, MINIM_CAR(it)->u.ptrs.p1, &val);
                    if (MINIM_OBJ_ERRORP(val))
                    {
                        res = val;
                        break;
                    }

                    if (MINIM_CDR(it))  free_minim_object(val);
                    else                res = fresh_minim_object(val);               
                }

                RELEASE_IF_REF(val);
                pop_env(env2);
            }
            else
            {
                eval_ast(env, MINIM_CADR(ce_pair)->u.ptrs.p1, &val);
                res = fresh_minim_object(val);
                RELEASE_IF_REF(val);
            }
        }

        free_minim_object(cond);
        free_minim_object(ce_pair);
    }

    if (argc == 0 || !eval)
        init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_unless(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_ERRORP(cond))
    {
        if (!coerce_into_bool(cond))
            res = minim_builtin_begin(env, &args[1], argc - 1);
        else
            init_minim_object(&res, MINIM_OBJ_VOID);

        free_minim_object(cond);
    }
    else
    {
        res = cond;
    }

    return res;
}

MinimObject *minim_builtin_when(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_ERRORP(cond))
    {
        if (coerce_into_bool(cond))
            res = minim_builtin_begin(env, &args[1], argc - 1);
        else
            init_minim_object(&res, MINIM_OBJ_VOID);

        free_minim_object(cond);
    }
    else
    {
        res = cond;
    }

    return res;
}

MinimObject *minim_builtin_def(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym, *val;

    unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        res = minim_argument_error("symbol", "def", 0, sym);
        free_minim_object(sym);
        return res;
    }

    if (argc == 2)
    {
        eval_ast(env, args[1]->u.ptrs.p1, &val);
    }
    else
    {
        MinimAst *ast = args[0]->u.ptrs.p1;
        MinimLambda *lam;
        
        val = minim_builtin_lambda(env, &args[1], argc - 1);
        
        lam = val->u.ptrs.p1;
        copy_syntax_loc(&lam->loc, ast->loc);
    }
    
    if (!MINIM_OBJ_ERRORP(val))
    {
        env_intern_sym(env, sym->u.str.str, val);
        init_minim_object(&res, MINIM_OBJ_VOID);
        RELEASE_IF_REF(val);
    }
    else
    {
        res = val;
    }

    free_minim_object(sym);
    return res;
}

MinimObject *minim_builtin_let(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res, *it;
    MinimEnv *env2;
    size_t len;
    bool err;
        
    // Convert bindings to list
    unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
    if (MINIM_OBJ_ERRORP(bindings))
    {
        res = bindings;
        return res;
    }

    len = minim_list_length(bindings);
    it = bindings;
    
    // Initialize child environment
    err = false;
    init_env(&env2, env);
    for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *val;

        unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            res = minim_argument_error("([<symbol> <value>] ...)", "let", SIZE_MAX, bind);
            free_minim_object(bindings);
            free_minim_object(bind);
            return res;
        }

        unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            res = minim_argument_error("symbol", "let", SIZE_MAX, sym);
            free_minim_object(bindings);
            free_minim_object(bind);
            free_minim_object(sym);
            return res;
        }
        
        eval_ast(env, MINIM_CADR(bind)->u.ptrs.p1, &val);
        env_intern_sym(env2, sym->u.str.str, val);
        RELEASE_IF_REF(val);

        free_minim_object(sym);
        free_minim_object(bind);
    }

    eval_ast(env2, args[1]->u.ptrs.p1, &it);
    res = fresh_minim_object(it);
    RELEASE_IF_REF(it);

    free_minim_object(bindings);
    pop_env(env2);

    return res;
}

MinimObject *minim_builtin_letstar(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res, *it;
    MinimEnv *env2;
    size_t len;
    bool err;

    // Convert bindings to list
    unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
    if (MINIM_OBJ_ERRORP(bindings))
    {
        res = bindings;
        return res;
    }

    len = minim_list_length(bindings);
    it = bindings;
    
    // Initialize child environment
    err = false;
    init_env(&env2, env);
    for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *val;

        unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            res = minim_argument_error("([<symbol> <value>] ...)", "let*", SIZE_MAX, bind);
            free_minim_object(bindings);
            free_minim_object(bind);
            return res;
        }

        unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            res = minim_argument_error("symbol", "let*", SIZE_MAX, sym);
            free_minim_object(bindings);
            free_minim_object(bind);
            free_minim_object(sym);
            return res;
        }
        
        eval_ast(env2, MINIM_CADR(bind)->u.ptrs.p1, &val);
        env_intern_sym(env2, sym->u.str.str, val);
        RELEASE_IF_REF(val);

        free_minim_object(sym);
        free_minim_object(bind);
    }

    eval_ast(env2, args[1]->u.ptrs.p1, &it);
    res = fresh_minim_object(it);
    RELEASE_IF_REF(it);

    free_minim_object(bindings);
    pop_env(env2);

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    unsyntax_ast_rec(env, args[0]->u.ptrs.p1, &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym, *val, *peek;

    unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        res = minim_argument_error("symbol", "set!", SIZE_MAX, sym);
        free_minim_object(sym);
        return res;
    }

    peek = env_peek_sym(env, sym->u.ptrs.p1);
    if (peek)
    {
        eval_ast(env, args[1]->u.ptrs.p1, &val);
        if (!MINIM_OBJ_ERRORP(val))
        {
            env_set_sym(env, sym->u.str.str, val);
            init_minim_object(&res, MINIM_OBJ_VOID);
        }
        else
        {
            res = val;
        }
    }
    else
    {
        minim_error(&res, "Variable not recognized '%s'", sym->u.ptrs.p1);
    }

    free_minim_object(sym);
    return res;
}

MinimObject *minim_builtin_begin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *val;
    MinimEnv *env2;

    init_env(&env2, env);
    for (size_t i = 0; i < argc; ++i)
    {
        eval_ast(env2, args[i]->u.ptrs.p1, &val);
        if (MINIM_OBJ_ERRORP(val))
        {
            res = val;
            break;
        }

        if (i + 1 == argc)      res = fresh_minim_object(val);
        else                    free_minim_object(val);
    }

    RELEASE_IF_REF(val);
    pop_env(env2);

    return res;
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SYMBOLP(args[0]));   
    return res;
}

MinimObject *minim_builtin_equalp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    for (size_t i = 1; i < argc; ++i)
    {
        if (!minim_equalp(args[i - 1], args[i]))
        {
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
            return res;
        }
    }

    init_minim_object(&res, MINIM_OBJ_BOOL, 1);
    return res;
}

MinimObject *minim_builtin_version(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    char *str;

    str = malloc((strlen(MINIM_VERSION_STR) + 1) * sizeof(char));
    strcpy(str, MINIM_VERSION_STR);
    init_minim_object(&res, MINIM_OBJ_STRING, str);

    return res;
}

MinimObject *minim_builtin_symbol_count(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    init_minim_number(&num, MINIM_NUMBER_EXACT);
    mpq_set_ui(num->rat, env->table->size, 1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);
    
    return res;
}
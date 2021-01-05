#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "builtin.h"
#include "bool.h"
#include "eval.h"
#include "list.h"
#include "variable.h"

bool minim_symbolp(MinimObject *obj)
{
    return (obj->type == MINIM_OBJ_SYM);   
}

bool assert_symbol(MinimObject *obj, MinimObject **res, const char* msg)
{
    if (obj->type != MINIM_OBJ_SYM)
    {
        minim_error(res, "%s", msg);
        return false;
    }

    return true;
}

MinimObject *minim_builtin_if(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    if (assert_exact_argc(&res, "if", 3, argc))
    {
        eval_ast(env, args[0]->data, &cond);

        if (cond->type != MINIM_OBJ_ERR)
        {
            if (coerce_into_bool(cond))     eval_ast(env, args[1]->data, &res);
            else                            eval_ast(env, args[2]->data, &res);

            free_minim_object(cond);
        }
        else
        {
            res = cond;
        }   
    }

    return res;
}

MinimObject *minim_builtin_def(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym, *val;

    if (assert_range_argc(&res, "def", 2, 3, argc))
    {
        unsyntax_ast(env, args[0]->data, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'def'"))
        {
            if (argc == 2)  eval_ast(env, args[1]->data, &val);
            else            val = minim_builtin_lambda(env, &args[1], 2);
            
            if (val->type != MINIM_OBJ_ERR)
            {
                env_intern_sym(env, ((char*) sym->data), val);
                init_minim_object(&res, MINIM_OBJ_VOID);
            }
            else
            {
                res = val;
            }
        }
        
        free_minim_object(sym);
    }

    return res;
}

MinimObject *minim_builtin_let(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res;
    MinimEnv *env2;

    if (assert_exact_argc(&res, "let", 2, argc))
    {
        MinimObject *it;
        size_t len;
        bool err = false;
        
        // Convert bindings to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (bindings->type == MINIM_OBJ_ERR)
        {
            res = bindings;
            return res;
        }
    
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2, env);
        for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
        {
            MinimObject *bind, *sym, *val;

            unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
            if (assert_list(bind, &res, "Expected ((symbol value) ...) in the bindings of 'let'") &&
                assert_list_len(bind, &res, 2, "Expected ((symbol value) ...) in the bindings of 'let'"))
            {
                unsyntax_ast(env, MINIM_CAR(bind)->data, &sym);
                if (assert_symbol(sym, &res, "Variable names must be symbols in let"))
                {
                    eval_ast(env, MINIM_CADR(bind)->data, &val);
                    env_intern_sym(env2, sym->data, val);
                    RELEASE_IF_REF(val);
                }
                else
                {
                    err = true;
                }

                free_minim_object(sym);
            }
            else
            {
                err = true;
            }

            free_minim_object(bind);
        }

        if (!err) eval_ast(env2, args[1]->data, &it);
        res = fresh_minim_object(it);
        RELEASE_IF_REF(it);

        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_letstar(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res;
    MinimEnv *env2;

    if (assert_exact_argc(&res, "let*", 2, argc))
    {
        MinimObject *it;
        size_t len;
        bool err = false;
        
        // Convert bindings to list
        unsyntax_ast(env, args[0]->data, &bindings);
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2, env);
        for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
        {
            MinimObject *bind, *sym, *val;

            unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
            if (assert_list(bind, &res, "Expected ((symbol value) ...) in the bindings of 'let*'") &&
                assert_list_len(bind, &res, 2, "Expected ((symbol value) ...) in the bindings of 'let*'"))
            {
                unsyntax_ast(env, MINIM_CAR(bind)->data, &sym);
                if (assert_symbol(sym, &res, "Variable names must be symbols in let*"))
                {
                    eval_ast(env2, MINIM_CADR(bind)->data, &val);
                    env_intern_sym(env2, sym->data, val);
                    RELEASE_IF_REF(val);
                }
                else
                {
                    err = true;
                }

                free_minim_object(sym);
            }
            else
            {
                err = true;
            }

            free_minim_object(bind);
        }

        if (!err) eval_ast(env2, args[1]->data, &it);
        res = fresh_minim_object(it);
        RELEASE_IF_REF(it);

        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "quote", 1, argc))
        unsyntax_ast_rec(env, args[0]->data, &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym;

    if (assert_exact_argc(&res, "set!", 2, argc))
    {
        unsyntax_ast(env, args[0]->data, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'set!'"))
        {
            MinimObject *val, *peek = env_peek_sym(env, sym->data);
            if (peek)
            {
                eval_ast(env, args[1]->data, &val);
                if (val->type != MINIM_OBJ_ERR)
                {
                    env_set_sym(env, sym->data, val);
                    init_minim_object(&res, MINIM_OBJ_VOID);
                }
                else
                {
                    res = val;
                }
            }
            else
            {
                minim_error(&res, "Variable not recognized '%s'", sym->data);
            }
        }

        free_minim_object(sym);
    }

    return res;
}

MinimObject *minim_builtin_begin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "begin", 1, argc))
    {
        MinimObject *val;
        MinimEnv *env2;

        init_env(&env2, env);
        for (size_t i = 0; i < argc; ++i)
        {
            eval_ast(env2, args[i]->data, &val);
            if (val->type == MINIM_OBJ_ERR)
            {
                res = val;
                break;
            }

            if (i + 1 == argc)      res = fresh_minim_object(val);
            else                    free_minim_object(val);
        }

        if (!MINIM_OBJ_OWNERP(val)) free_minim_object(val);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "symbol?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, args[0]->type == MINIM_OBJ_SYM);
        
    return res;
}

MinimObject *minim_builtin_equalp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "equal?", 1, argc))
    {
        for (size_t i = 1; i < argc; ++i)
        {
            if (!minim_equalp(args[i - 1], args[i]))
            {
                init_minim_object(&res, MINIM_OBJ_BOOL, 0);
                return res;
            }
        }

        init_minim_object(&res, MINIM_OBJ_BOOL, 1);
    }

    return res;
}
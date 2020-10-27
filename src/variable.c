#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "bool.h"
#include "eval.h"
#include "lambda.h"
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

void env_load_module_variable(MinimEnv *env)
{
    env_load_builtin(env, "if", MINIM_OBJ_SYNTAX, minim_builtin_if);
    env_load_builtin(env, "def", MINIM_OBJ_SYNTAX, minim_builtin_def);
    env_load_builtin(env, "let", MINIM_OBJ_SYNTAX, minim_builtin_let);
    env_load_builtin(env, "let*", MINIM_OBJ_SYNTAX, minim_builtin_letstar);
    env_load_builtin(env, "quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);

    env_load_builtin(env, "set!", MINIM_OBJ_SYNTAX, minim_builtin_setb);
    
    env_load_builtin(env, "symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
}

MinimObject *minim_builtin_if(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *cond;

    if (assert_exact_argc(argc, args, &res, "if", 3))
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

MinimObject *minim_builtin_def(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *sym, *val;

    if (assert_range_argc(argc, args, &res, "def", 2, 3))
    {
        unsyntax_ast(env, args[0]->data, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'def'"))
        {
            if (argc == 2)
            {
                eval_ast(env, args[1]->data, &val);
            }
            else
            {
                MinimObject **lam_args = malloc(2 * sizeof(MinimObject*));
                copy_minim_object(&lam_args[0], args[1]);
                copy_minim_object(&lam_args[1], args[2]);
                val = minim_builtin_lambda(env, 2, lam_args);
                free_minim_objects(2, lam_args);
            }

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

MinimObject *minim_builtin_let(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *bindings, *res = NULL;
    MinimEnv *env2;

    if (assert_exact_argc(argc, args, &res, "let", 2))
    {
        MinimObject *it;
        int len;
        
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
        init_env(&env2);        
        env2->parent = env;
        
        for (int i = 0; !res && i < len; ++i, it = MINIM_CDR(it))
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
                }

                free_minim_object(sym);
            }

            free_minim_object(bind);
        }

        if (!res) eval_ast(env2, args[1]->data, &res);
        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_letstar(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *bindings, *res = NULL;
    MinimEnv *env2;

    if (assert_exact_argc(argc, args, &res, "let*", 2))
    {
        MinimObject *it;
        int len;
        bool err = false;
        
        // Convert bindings to list
        unsyntax_ast(env, args[0]->data, &bindings);
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2);        
        env2->parent = env;
        
        for (int i = 0; !res && i < len; ++i, it = MINIM_CDR(it))
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
                }

                free_minim_object(sym);
            }

            free_minim_object(bind);
        }

        if (!err) eval_ast(env2, args[1]->data, &res);
        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "quote", 1))
        unsyntax_ast(env, args[0]->data, &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *sym;

    if (assert_exact_argc(argc, args, &res, "set!", 2))
    {
        unsyntax_ast(env, args[0]->data, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'set!'"))
        {
            MinimObject *val, *peek = env_peek_sym(env, sym->data);
            if (peek)
            {
                eval_ast(env, args[1]->data, &val);
                env_set_sym(env, sym->data, val);
                init_minim_object(&res, MINIM_OBJ_VOID);
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

MinimObject *minim_builtin_symbolp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "symbol?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, args[0]->type == MINIM_OBJ_SYM);
    return res;
}
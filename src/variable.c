#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "eval.h"
#include "list.h"
#include "print.h"
#include "variable.h"

void env_load_module_variable(MinimEnv *env)
{
    env_load_builtin(env, "def", MINIM_OBJ_SYNTAX, minim_builtin_def);
    env_load_builtin(env, "let", MINIM_OBJ_SYNTAX, minim_builtin_let);
    env_load_builtin(env, "let*", MINIM_OBJ_SYNTAX, minim_builtin_letstar);
    env_load_builtin(env, "quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);

    env_load_builtin(env, "number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    env_load_builtin(env, "symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
}

MinimObject *minim_builtin_def(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *sym, *val;

    if (assert_range_argc(argc, args, &res, "def", 2, 2)) // TODO: def-fun: 3 args
    {
        eval_ast_as_quote(env, args[0]->data, &sym);
        if (assert_sym_arg(sym, &res, "def"))
        {
            eval_ast(env, args[1]->data, &val);
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

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_let(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *bindings;
    MinimEnv *env2;

    if (assert_exact_argc(argc, args, &res, "let", 2))
    {
        MinimObject *it, *it2, *val;
        char *sym;
        int len;
        
        // Convert bindings to list
        eval_ast_as_quote(env, args[0]->data, &bindings);
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2);        
        env2->next = env;
        
        for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
        {
            it2 = MINIM_CAR(it);
            if (minim_list_length(it2) != 2)
            {
                minim_error(&res, "Expected a symbol and value in binding %d of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else if (MINIM_CAR(it2)->type != MINIM_OBJ_SYM)
            {
                minim_error(&res, "Expected a symbol in binding %d of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else if (MINIM_CADR(it2)->type == MINIM_OBJ_VOID)
            {
                minim_error(&res, "Expected a non-void value in the %d binding of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else
            {
                sym = ((char*) MINIM_CAR(it2)->data);
                eval_sexpr(env, MINIM_CADR(it2), &val);
                env_intern_sym(env2, sym, val);
            }
        }

        eval_ast(env2, args[1]->data, &res);
        free_minim_object(bindings);
        pop_env(env2);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_letstar(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *bindings;
    MinimEnv *env2;

    if (assert_exact_argc(argc, args, &res, "let", 2))
    {
        MinimObject *it, *it2, *val;
        char *sym;
        int len;
        
        // Convert bindings to list
        eval_ast_as_quote(env, args[0]->data, &bindings);
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2);        
        env2->next = env;
        
        for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
        {
            it2 = MINIM_CAR(it);
            if (minim_list_length(it2) != 2)
            {
                minim_error(&res, "Expected a symbol and value in binding %d of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else if (MINIM_CAR(it2)->type != MINIM_OBJ_SYM)
            {
                minim_error(&res, "Expected a symbol in binding %d of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else if (MINIM_CADR(it2)->type == MINIM_OBJ_VOID)
            {
                minim_error(&res, "Expected a non-void value in the %d binding of \'let\'", i + 1);
                free_minim_objects(argc, args);
                free_minim_object(bindings);
                pop_env(env2);
                return res;
            }
            else
            {
                sym = ((char*) MINIM_CAR(it2)->data);
                eval_sexpr(env2, MINIM_CADR(it2), &val);
                env_intern_sym(env2, sym, val);
            }
        }

        eval_ast(env2, args[1]->data, &res);
        free_minim_object(bindings);
        pop_env(env2);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "quote", 1))
        eval_ast_as_quote(env, args[0]->data, &res);

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_numberp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "number?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, args[0]->type == MINIM_OBJ_NUM);
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "symbol?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, args[0]->type == MINIM_OBJ_SYM);
    free_minim_objects(argc, args);
    return res;
}
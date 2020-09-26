#include <stddef.h>
#include "assert.h"
#include "eval.h"
#include "variable.h"

void env_load_module_variable(MinimEnv *env)
{
    env_load_builtin(env, "def", MINIM_OBJ_SYNTAX, minim_builtin_def);
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

MinimObject *minim_builtin_quote(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "quote", 1))
    {
        eval_ast_as_quote(env, args[0]->data, &res);
        args[0] = NULL;
    }

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
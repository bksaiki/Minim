#include <stdlib.h>

#include "assert.h"
#include "eval.h"
#include "for.h"
#include "iter.h"
#include "list.h"
#include "variable.h"

static bool iters_valid(int argc, MinimIter **iters)
{
    for (int i = 0; i < argc; ++i)
    {
        if (minim_iter_endp(iters[i]))
            return false;
    }

    return true;
}

static void free_iters(int argc, MinimIter **iters)
{
    for (int i = 0; i < argc; ++i)
        free_minim_iter(iters[i]);
}

static bool try_iter_no_copy(MinimObject *obj, MinimEnv *env, MinimIter **piter)
{
    MinimObject *val;

    if (obj->type == MINIM_OBJ_SYM)
    {
        val = env_peek_sym(env, obj->data);
        if (val && minim_is_iterable(val))
        {
            init_minim_iter(piter, val);
            return true;
        }
    }

    return false;
}

// Builtins

void env_load_module_for(MinimEnv *env)
{
    env_load_builtin(env, "for", MINIM_OBJ_SYNTAX, minim_builtin_for);
}

MinimObject *minim_builtin_for(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "for", 2))
    {
        MinimObject *it, *bindings;

        // Convert iter/iterable pairs to list
        eval_ast_as_quote(env, args[0]->data, &bindings);
        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'for'"))
        {
            MinimObject *bind, *val;
            MinimIter **iters;
            MinimIterObjs *iobjs;
            char **syms;
            int len;
            bool err = false;
            
            it = bindings;
            len = minim_list_length(bindings);
            iters = malloc(len * sizeof(MinimIter*));
            syms = malloc(len * sizeof(char*));
            iobjs = env_get_iobjs(env);

            for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
            {
                bind = MINIM_CAR(it);
                if (!assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'for'") ||
                    !assert_list_len(bind, &res, 2, "Expected a valid binding '(name value)' in the bindings of 'for'") ||
                    !assert_symbol(MINIM_CAR(bind), &res, "Expected a symbol for a variable name in the bindings of 'for'"))
                {
                    err = true;
                    break;
                }
                else
                {
                    syms[i] = MINIM_CAR(bind)->data;
                    if (!try_iter_no_copy(MINIM_CADR(bind), env, &iters[i]))
                    {
                        eval_sexpr(env, MINIM_CADR(bind), &val);
                        if (!assert_minim_iterable(val, &res, "Expected an iterable object in the value of a binding in 'for'"))
                        {
                            free_minim_object(val);
                            err = true;
                            break;
                        }

                        init_minim_iter(&iters[i], val);
                        minim_iter_objs_add(iobjs, iters[i]);
                    }
                }
            }

            if (!err)
            {
                while(iters_valid(len, iters))
                {
                    MinimEnv *env2;
                    MinimObject *ast;

                    init_env(&env2);
                    env2->parent = env;

                    for (int i = 0; i < len; ++i)
                    {
                        copy_minim_object(&val, minim_iter_peek(iters[i]));
                        env_intern_sym(env2, syms[i], val);
                        minim_iter_next(iters[i]);
                    }

                    copy_minim_object(&ast, args[1]);
                    eval_ast(env2, ast->data, &val);

                    if (val->type == MINIM_OBJ_ERR)
                    {
                        res = val;
                        free_minim_object(ast);
                        pop_env(env2);
                        err = true;
                        break;
                    }
                    else
                    {
                        free_minim_object(val);
                        free_minim_object(ast);
                        pop_env(env2);
                    }
                }   

                if (!err) init_minim_object(&res, MINIM_OBJ_VOID);
                free_iters(len, iters);
            }

            free(iters);
            free(syms);
        }

        free_minim_object(bindings);
    }

    free_minim_objects(argc, args);
    return res;
}
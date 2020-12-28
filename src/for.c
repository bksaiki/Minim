#include <stdlib.h>

#include "assert.h"
#include "eval.h"
#include "for.h"
#include "iter.h"
#include "list.h"
#include "variable.h"

static bool iters_valid(MinimObject **iters, int argc)
{
    for (int i = 0; i < argc; ++i)
    {
        if (minim_iter_endp(iters[i]))
            return false;
    }

    return true;
}

// Builtins

void env_load_module_for(MinimEnv *env)
{
    env_load_builtin(env, "for", MINIM_OBJ_SYNTAX, minim_builtin_for);
    env_load_builtin(env, "for-list", MINIM_OBJ_SYNTAX, minim_builtin_for_list);
}

MinimObject *minim_builtin_for(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "for", 2, argc))
    {
        MinimObject *it, *bindings;

        // Convert iter/iterable pairs to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'for'"))
        {
            MinimObject **syms, **iters, **objs;
            MinimObject *val;
            int len;
            bool err = false;
            
            it = bindings;
            len = minim_list_length(bindings);
            objs = malloc(len * sizeof(MinimObject*));
            iters = malloc(len * sizeof(MinimObject*));
            syms = malloc(len * sizeof(MinimObject*));

            for (int i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
            {
                MinimObject *bind;

                unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
                if (assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'for'") &&
                    assert_list_len(bind, &res, 2, "Expected a valid binding '(name value)' in the bindings of 'for'"))
                {
                    unsyntax_ast(env, MINIM_CAR(bind)->data, &syms[i]);
                    if (assert_symbol(syms[i], &res, "Expected a symbol for a variable name in the bindings of 'for'"))
                    {
                        eval_ast(env, MINIM_CADR(bind)->data, &val);
                        if (assert_generic(&res, "Expected an iterable object in the bindings of 'for'", minim_iterablep(val)))
                        {
                            objs[i] = val;
                            ref_minim_object(&iters[i], objs[i]);
                        }
                        else
                        {
                            err = true;
                            free_minim_object(val);
                        }
                    }
                    else
                    {
                        err = true;
                    }

                    if (err) free_minim_object(syms[i]);
                }
                else
                {
                    err = true;
                }

                free_minim_object(bind);
                if (err)
                {
                    free_minim_objects(syms, i);
                    free_minim_objects(objs, i);
                    free_minim_objects(iters, i);
                    free_minim_object(bindings);
                    return res;
                }
            }

            while(!err && iters_valid(iters, len))
            {
                MinimEnv *env2;

                init_env(&env2);
                env2->parent = env;

                for (int i = 0; i < len; ++i)
                {
                    val = minim_iter_get(iters[i]);
                    env_intern_sym(env2, syms[i]->data, val);
                    RELEASE_IF_REF(val);
                }

                eval_ast(env2, args[1]->data, &val);
                if (val->type == MINIM_OBJ_ERR)
                {
                    res = val;
                    pop_env(env2);
                    err = true;
                }
                else
                {
                    free_minim_object(val);
                    pop_env(env2);

                    for (int i = 0; i < len; ++i)
                        iters[i] = minim_iter_next(iters[i]);
                }
            }   

            if (!err) init_minim_object(&res, MINIM_OBJ_VOID);
            free_minim_objects(iters, len);
            free_minim_objects(objs, len);
            free_minim_objects(syms, len);
        }

        free_minim_object(bindings);
    }

    return res;
}

MinimObject *minim_builtin_for_list(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "for", 2, argc))
    {
        MinimObject *it, *bindings;

        // Convert iter/iterable pairs to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'for'"))
        {
            MinimObject **syms, **iters, **objs;
            MinimObject *val;
            int len;
            bool head = true, err = false;
            
            it = bindings;
            len = minim_list_length(bindings);
            objs = malloc(len * sizeof(MinimObject*));
            iters = malloc(len * sizeof(MinimObject*));
            syms = malloc(len * sizeof(MinimObject*));

            for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
            {
                MinimObject *bind;

                unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
                if (assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'for'") &&
                    assert_list_len(bind, &res, 2, "Expected a valid binding '(name value)' in the bindings of 'for'"))
                {
                    unsyntax_ast(env, MINIM_CAR(bind)->data, &syms[i]);
                    if (assert_symbol(syms[i], &res, "Expected a symbol for a variable name in the bindings of 'for'"))
                    {
                        eval_ast(env, MINIM_CADR(bind)->data, &val);
                        if (assert_generic(&res, "Expected an iterable object in the bindings of 'for-list'", minim_iterablep(val)))
                        {
                            objs[i] = val;
                            ref_minim_object(&iters[i], objs[i]);
                        }
                        else
                        {
                            err = true;
                            free_minim_object(val);
                        }
                    }
                    else
                    {
                        err = true;
                    }

                    if (err) free_minim_object(syms[i]);
                }
                else
                {
                    err = true;
                }

                free_minim_object(bind);
                if (err)
                {
                    free_minim_objects(syms, i);
                    free_minim_objects(objs, i);
                    free_minim_objects(iters, i);
                    free_minim_object(bindings);
                    return res;
                }
            }

            while(!err && iters_valid(iters, len))
            {
                MinimEnv *env2;

                init_env(&env2);
                env2->parent = env;

                for (int i = 0; i < len; ++i)
                {
                    val = minim_iter_get(iters[i]);
                    env_intern_sym(env2, syms[i]->data, val);
                    RELEASE_IF_REF(val);
                }

                eval_ast(env2, args[1]->data, &val);
                if (val->type == MINIM_OBJ_ERR)
                {
                    res = val;
                    pop_env(env2);
                    err = true;
                }
                else
                {
                    if (head)
                    {
                        init_minim_object(&res, MINIM_OBJ_PAIR, fresh_minim_object(val), NULL);
                        RELEASE_IF_REF(val);
                        it = res;
                        head = false;
                    }
                    else
                    {
                        init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, fresh_minim_object(val), NULL);
                        RELEASE_IF_REF(val);
                        it = MINIM_CDR(it);
                    }

                    pop_env(env2);
                    for (int i = 0; i < len; ++i)
                        iters[i] = minim_iter_next(iters[i]);
                }
            }   

            if (head && !err) init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
            free_minim_objects(iters, len);
            free_minim_objects(objs, len);
            free_minim_objects(syms, len);
        }

        free_minim_object(bindings);
    }

    return res;
}
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
    env_load_builtin(env, "for-list", MINIM_OBJ_SYNTAX, minim_builtin_for_list);
}

MinimObject *minim_builtin_for(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res = NULL;

    if (assert_exact_argc(argc, &res, "for", 2))
    {
        MinimObject *it, *bindings;

        // Convert iter/iterable pairs to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'for'"))
        {
            MinimObject *val;
            MinimIter **iters;
            MinimIterObjs *iobjs;
            char **syms;
            int len;
            
            it = bindings;
            len = minim_list_length(bindings);
            iters = malloc(len * sizeof(MinimIter*));
            syms = malloc(len * sizeof(char*));
            iobjs = env_get_iobjs(env);

            for (int i = 0; !res && i < len; ++i, it = MINIM_CDR(it))
            {
                MinimObject *bind, *sym;

                unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
                if (assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'for'") &&
                    assert_list_len(bind, &res, 2, "Expected a valid binding '(name value)' in the bindings of 'for'"))
                {
                    unsyntax_ast(env, MINIM_CAR(bind)->data, &sym);
                    if (assert_symbol(sym, &res, "Expected a symbol for a variable name in the bindings of 'for'"))
                    {
                        syms[i] = sym->data;
                        if (!try_iter_no_copy(MINIM_CADR(bind), env, &iters[i]))
                        {
                            eval_ast(env, MINIM_CADR(bind)->data, &val);
                            if (assert_minim_iterable(val, &res, "Expected an iterable object in the value of a binding in 'for'"))
                            {
                                init_minim_iter(&iters[i], val);
                                minim_iter_objs_add(iobjs, iters[i]);
                            }
                            else
                            {
                                free_minim_object(val);
                            }
                        }
                    }

                    sym->data = NULL;
                    free_minim_object(sym);
                }

                free_minim_object(bind);
            }

            if (!res)
            {
                while(iters_valid(len, iters))
                {
                    MinimEnv *env2;

                    init_env(&env2);
                    env2->parent = env;

                    for (int i = 0; i < len; ++i)
                    {
                        copy_minim_object(&val, minim_iter_peek(iters[i]));
                        env_intern_sym(env2, syms[i], val);
                        minim_iter_next(iters[i]);
                    }

                    eval_ast(env2, args[1]->data, &val);
                    if (val->type == MINIM_OBJ_ERR)
                    {
                        res = val;
                        pop_env(env2);
                        break;
                    }
                    else
                    {
                        free_minim_object(val);
                        pop_env(env2);
                    }
                }   

                if (!res) init_minim_object(&res, MINIM_OBJ_VOID);
                free_iters(len, iters);
            }

            for (int i = 0; i < len; ++i)
                free(syms[i]);
                
            free(syms);
            free(iters);
        }

        free_minim_object(bindings);
    }

    return res;
}

MinimObject *minim_builtin_for_list(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res = NULL;

    if (assert_exact_argc(argc, &res, "for-list", 2))
    {
        MinimObject *it, *bindings;

        // Convert iter/iterable pairs to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'for-list'"))
        {
            MinimObject *val;
            MinimIter **iters;
            MinimIterObjs *iobjs;
            char **syms;
            int len;
            
            it = bindings;
            len = minim_list_length(bindings);
            iters = malloc(len * sizeof(MinimIter*));
            syms = malloc(len * sizeof(char*));
            iobjs = env_get_iobjs(env);

            for (int i = 0; !res && i < len; ++i, it = MINIM_CDR(it))
            {
                MinimObject *bind, *sym;

                unsyntax_ast(env, MINIM_CAR(it)->data, &bind);
                if (assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'for-list'") &&
                    assert_list_len(bind, &res, 2, "Expected a valid binding '(name value)' in the bindings of 'for-list'"))
                {
                    unsyntax_ast(env, MINIM_CAR(bind)->data, &sym);
                    if (assert_symbol(sym, &res, "Expected a symbol for a variable name in the bindings of 'for-list'"))
                    {
                        syms[i] = sym->data;
                        if (!try_iter_no_copy(MINIM_CADR(bind), env, &iters[i]))
                        {
                            eval_ast(env, MINIM_CADR(bind)->data, &val);
                            if (assert_minim_iterable(val, &res, "Expected an iterable object in the value of a binding in 'for-list'"))
                            {
                                init_minim_iter(&iters[i], val);
                                minim_iter_objs_add(iobjs, iters[i]);
                            }
                            else
                            {
                                free_minim_object(val);
                            }
                        }
                    }

                    sym->data = NULL;
                    free_minim_object(sym);
                }

                free_minim_object(bind);
            }

            if (!res)
            {   
                MinimObject *next, *prev = NULL;
                bool err = false;

                while(!err && iters_valid(len, iters))
                {
                    MinimEnv *env2;

                    init_env(&env2);
                    env2->parent = env;

                    for (int i = 0; i < len; ++i)
                    {
                        copy_minim_object(&val, minim_iter_peek(iters[i]));
                        env_intern_sym(env2, syms[i], val);
                        minim_iter_next(iters[i]);
                    }

                    eval_ast(env2, args[1]->data, &val);
                    if (val->type == MINIM_OBJ_ERR)
                    {
                        free_minim_object(res);
                        pop_env(env2);

                        res = val;
                        err = true;
                        break;
                    }
                    else
                    {
                        init_minim_object(&next, MINIM_OBJ_PAIR, val, NULL);
                        if (prev) MINIM_CDR(prev) = next;
                        else      res = next;

                        prev = next;
                        pop_env(env2);
                    }
                }   

                if (!res) init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
                free_iters(len, iters);
            }

            for (int i = 0; i < len; ++i)
                free(syms[i]);

            free(syms);
            free(iters);
        }

        free_minim_object(bindings);
    }

    return res;
}
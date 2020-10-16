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
        int len;

        // Convert iter/iterable pairs to list
        eval_ast_as_quote(env, args[0]->data, &bindings);
        len = minim_list_length(bindings);
        it = bindings;

        if (assert_list(bindings, &res, "Expected ((iter iterable) ...) in the 1st argument of 'let'"))
        {
            MinimObject *bind, *val;
            MinimIter **iters;
            char **syms;
            bool err = false;
            
            it = bindings;
            len = minim_list_length(bindings);
            iters = malloc(len * sizeof(MinimIter*));
            syms = malloc(len * sizeof(char*));

            for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
            {
                bind = MINIM_CAR(it);
                if (!assert_list(bind, &res, "Expected a valid binding in the bindings in the 1st argument of 'let'") ||
                    !assert_list_len(bind, &res, 2, "A valid binding must contain a name and value") ||
                    !assert_symbol(MINIM_CAR(bind), &res, "A binding name must be a symbol"))
                {
                    err = true;
                    break;
                }
                else
                {
                    eval_sexpr(env, MINIM_CADR(bind), &val);
                    if (!assert_minim_iterable(val, &res, "Expected an iterable object in the value of a binding in 'for'"))
                    {
                        free_minim_object(val);
                        err = true;
                        break;
                    }

                    init_minim_iter(&iters[i], val);
                    syms[i] = MINIM_CAR(bind)->data;
                }

            }

            if (!err)
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
                    free_minim_object(val);
                    pop_env(env2);
                }   

                init_minim_object(&res, MINIM_OBJ_VOID);
            }

            free(iters);
        }

        free_minim_object(bindings);
    }

    free_minim_objects(argc, args);
    return res;
}
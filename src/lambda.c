#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "variable.h"

void init_minim_lambda(MinimLambda **plam)
{
    MinimLambda *lam = malloc(sizeof(MinimLambda));
    lam->name = NULL;
    lam->formals = NULL;
    lam->body = NULL;
    *plam = lam;
}

void copy_minim_lambda(MinimLambda **cp, MinimLambda *src)
{
    MinimLambda *lam = malloc(sizeof(MinimLambda));

    if (src->name)
    {
        lam->name = malloc((strlen(src->name) + 1) * sizeof(char));
        strcpy(lam->name, src->name);
    }
    else
    {
        lam->name = NULL;
    }

    if (src->formals)   copy_minim_object(&lam->formals, src->formals);
    else                lam->formals = NULL;

    if (src->body)      copy_minim_object(&lam->body, src->body);
    else                lam->body = NULL;

    *cp = lam;
}

void free_minim_lambda(MinimLambda *lam)
{
    if (lam->name)      free(lam->name);
    if (lam->formals)   free_minim_object(lam->formals);
    if (lam->body)      free_minim_object(lam->body);

    free(lam);
}

MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res, *val;
    MinimEnv *env2;
    char *sym;

    if (minim_listp(lam->formals))
    {
        MinimObject *it;
        int len = minim_list_length(lam->formals);

        if (assert_exact_argc(argc, args, &res, ((lam->name) ? lam->name : "unnamed lambda"), len))
        {   
            init_env(&env2);
            env2->parent = env;

            it = lam->formals;
            for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
            {
                sym = ((char*) MINIM_CAR(it)->data);
                env_intern_sym(env2, sym, args[i]);
                args[i] = NULL;
            }


            eval_ast(env2, lam->body->data, &res);
            pop_env(env2);
        }
    }
    else
    {
        init_env(&env2);
        env2->parent = env;

        val = minim_construct_list(argc, args);
        env_intern_sym(env2, lam->formals->data, val);
        for (int i = 0; i < argc; ++i)  args[i] = NULL;

        eval_ast(env2, lam->body->data, &res);
        pop_env(env2);
    }
    
    return res;
}

bool assert_func(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_funcp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

void env_load_module_lambda(MinimEnv *env)
{
    env_load_builtin(env, "lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
}

MinimObject *minim_builtin_lambda(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_range_argc(argc, args, &res, "lambda", 2, 2)) // TODO: def-fun: 3 args
    {
        MinimObject *bindings;
        MinimLambda *lam;

        // Convert bindings to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (bindings->type != MINIM_OBJ_ERR)
        {
            if (minim_symbolp(bindings) || minim_listof(bindings, minim_symbolp))
            {
                init_minim_lambda(&lam);
                lam->formals = bindings;
                copy_minim_object(&lam->body, args[1]);
                init_minim_object(&res, MINIM_OBJ_CLOSURE, lam);
            }
            else
            {
                minim_error(&res, "Expected a symbol or list of symbols for lambda variables");
                free_minim_object(bindings);
            }
        }
        else
        {
            res = bindings;
        }
    }

    return res;
}
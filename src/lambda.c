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

    init_minim_lambda(&lam);
    if (src->name)
    {
        lam->name = malloc((strlen(src->name) + 1) * sizeof(char));
        strcpy(lam->name, src->name);
    }

    if (lam->formals)   copy_minim_object(&lam->formals, src->formals);
    if (lam->body)      copy_minim_object(&lam->body, src->body);

    *cp = lam;
}

void free_minim_lambda(MinimLambda *lam)
{
    if (lam->name)      free(lam->name);
    if (lam->formals)   free_minim_object(lam->formals);
    if (lam->body)      free_minim_object(lam->body);

    free(lam);
}

void env_load_module_lambda(MinimEnv *env)
{
    env_load_builtin(env, "lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
}

MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;
    int len;

    len = minim_list_length(lam->formals);
    if (assert_exact_argc(argc, args, &res, ((lam->name) ? lam->name : "?"), len))
    {   
        MinimEnv *env2;
        MinimObject *it, *val;
        char *sym;

        init_env(&env2);
        env2->parent = env;

        it = lam->formals;
        for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
        {
            sym = ((char*) MINIM_CAR(it)->data);
            copy_minim_object(&val, args[i]);
            env_intern_sym(env2, sym, val);
        }

        eval_ast(env2, lam->body->data, &res);
        pop_env(env2);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_lambda(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_range_argc(argc, args, &res, "lambda", 2, 2)) // TODO: def-fun: 3 args
    {
        MinimObject *bindings;
        MinimLambda *lam;

        // Convert bindings to list
        eval_ast_as_quote(env, args[0]->data, &bindings);
        if (bindings->type != MINIM_OBJ_ERR)
        {
            if (assert_listof(bindings, &res, minim_symbolp, "Lambda variables must only contain symbols"))
            {
                init_minim_lambda(&lam);
                lam->formals = bindings;
                lam->body = args[1];
                args[1] = NULL;

                init_minim_object(&res, MINIM_OBJ_CLOSURE, lam);
            }
            else
            {
                free_minim_object(bindings);
            }
        }
        else
        {
            res = bindings;
        }
    }

    free_minim_objects(argc, args);
    return res;
}
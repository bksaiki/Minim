#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "math.h"

//
//  Visible functions
//

void env_load_module_math(MinimEnv *env)
{
    env_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    env_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_mul);
    env_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_sub);
    env_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);
}

MinimObject *minim_builtin_add(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "+") &&
        assert_min_argc(argc, args, &res, "+", 1))
    {
        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum += *pval;
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_sub(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "-") &&
        assert_min_argc(argc, args, &res, "-", 1))
    {    
        if (argc == 1)
        {
            init_minim_object(&res, MINIM_OBJ_NUM, - *((int*)args[0]->data));
        }
        else
        {
            init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
            for (int i = 1; i < argc; ++i)
            {
                int *accum = (int*)res->data;
                int *pval = (int*)args[i]->data;
                *accum -= *pval;
            }
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_mul(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "*") &&
        assert_min_argc(argc, args, &res, "*", 1))
    {
        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum *= *pval;
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_div(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "/") &&
        assert_exact_argc(argc, args, &res, "/", 2))
    {
        int num = *((int*)args[0]->data);
        int den = *((int*)args[1]->data);

        if (*((int*) args[1]->data) == 0)
            init_minim_object(&res, MINIM_OBJ_ERR, "Divison by zero");
        else
            init_minim_object(&res, MINIM_OBJ_NUM, num / den);
    }

    free_minim_objects(argc, args);
    return res;
}

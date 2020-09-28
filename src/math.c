#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "math.h"
#include "number.h"

//
//  Visible functions
//

void env_load_module_math(MinimEnv *env)
{
    env_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    env_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_sub);
    env_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_mul);
    env_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);

    env_load_builtin(env, "zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
}

MinimObject *minim_builtin_add(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "+") &&
        assert_min_argc(argc, args, &res, "+", 1))
    {
        copy_minim_object(&res, args[0]);
        for (int i = 1; i < argc; ++i)
            minim_number_add(res->data, res->data, args[i]->data);
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
            MinimNumber *neg;
            init_exact_number(&neg);
            minim_number_neg(neg, args[0]->data);
            init_minim_object(&res, MINIM_OBJ_NUM, neg);
        }
        else
        {
            copy_minim_object(&res, args[0]);
            for (int i = 1; i < argc; ++i)
                minim_number_sub(res->data, res->data, args[i]->data);
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
        copy_minim_object(&res, args[0]);
        for (int i = 1; i < argc; ++i)
            minim_number_mul(res->data, res->data, args[i]->data);
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
        if (minim_number_zerop(args[1]->data))
        {
            minim_error(&res, "Division by zero");
        }
        else
        {
            copy_minim_object(&res, args[0]);
            minim_number_div(res->data, res->data, args[1]->data);
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_zerop(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_numerical_args(argc, args, &res, "zero?") &&
        assert_exact_argc(argc, args, &res, "zero?", 1))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, *((int*) args[0]->data) == 0);
    }

    free_minim_objects(argc, args);
    return res;
}
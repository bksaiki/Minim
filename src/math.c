#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "math.h"
#include "number.h"

// Internals

static void minim_number_neg(MinimNumber *res, MinimNumber *a)
{
    if (a->type == MINIM_NUMBER_EXACT)  mpq_neg(res->rat, a->rat);
    else                                res->fl = - a->fl;
}

static void minim_number_add(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_add(res->rat, a->rat, b->rat);
        res->type = MINIM_NUMBER_EXACT;
    }
    else
    {
        res->fl = (((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl) +
                   ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl));
        res->type = MINIM_NUMBER_INEXACT;
    }
}

static void minim_number_sub(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_sub(res->rat, a->rat, b->rat);
        res->type = MINIM_NUMBER_EXACT;
    }
    else
    {
        res->fl = (((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl) -
                   ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl));
        res->type = MINIM_NUMBER_INEXACT;
    }
}

static void minim_number_mul(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_mul(res->rat, a->rat, b->rat);
        res->type = MINIM_NUMBER_EXACT;
    }
    else
    {
        res->fl = (((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl) *
                   ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl));
        res->type = MINIM_NUMBER_INEXACT;
    }
}

static void minim_number_div(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_div(res->rat, a->rat, b->rat);
        res->type = MINIM_NUMBER_EXACT;
    }
    else
    {
        res->fl = (((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl) /
                   ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl));
        res->type = MINIM_NUMBER_INEXACT;
    }
}

// *** Builtins *** //

void env_load_module_math(MinimEnv *env)
{
    env_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    env_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_sub);
    env_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_mul);
    env_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);
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
            minim_number_neg(args[0]->data, args[0]->data);
        }
        else
        {
            for (int i = 1; i < argc; ++i)
                minim_number_sub(args[0]->data, args[0]->data, args[i]->data);
        
        }
    }

    res = args[0];
    args[0] = NULL;
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
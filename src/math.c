#include <math.h>
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
    double fa, fb;

    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_add(res->rat, a->rat, b->rat);
    }
    else
    {
        fa = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        fb = ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl);

        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = fa + fb;
    }
}

static void minim_number_sub(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    double fa, fb;

    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_sub(res->rat, a->rat, b->rat);
    }
    else
    {
        fa = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        fb = ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl);

        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = fa - fb;
    }
}

static void minim_number_mul(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    double fa, fb;

    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_mul(res->rat, a->rat, b->rat);
    }
    else
    {
        fa = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        fb = ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl);

        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = fa * fb;
    }
}

static void minim_number_div(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    double fa, fb;

    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_div(res->rat, a->rat, b->rat);
    }
    else
    {
        fa = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        fb = ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl);

        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = fa / fb;
    }
}

static void minim_number_sqrt(MinimNumber *res, MinimNumber *a)
{
    if (minim_number_negativep(a))
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = NAN;
    }
    else if (a->type == MINIM_NUMBER_EXACT)
    {
        mpz_t num, den;

        mpz_inits(num, den, NULL);
        mpq_get_num(num, a->rat);
        mpq_get_den(den, a->rat);

        if (mpz_cmpabs_ui(den, 1) == 0)
        {
            mpz_sqrtrem(num, den, num);   
            if (mpz_cmp_ui(den, 0) == 0)
            {
                reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
                mpq_set_z(res->rat, num);
            }
            else
            {
                reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
                res->fl = sqrt(mpq_get_d(a->rat));
            }
        }
        else
        {
            reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
            res->fl = sqrt(mpq_get_d(a->rat));
        }

        mpz_clears(num, den, NULL);
    }
    else        
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = sqrt(a->fl);
    }
}

static void minim_number_exp(MinimNumber *res, MinimNumber *a)
{
    double f;

    if (a->type == MINIM_NUMBER_EXACT && minim_number_zerop(a))
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_si(res->rat, 1, 1);
    }
    else
    {
        f = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = exp(f);
    }
}

static void minim_number_log(MinimNumber *res, MinimNumber *a)
{
    double f;

    if (minim_number_zerop(a) || minim_number_negativep(a))
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = NAN;
    }
    else if (a->type == MINIM_NUMBER_EXACT && mpq_cmp_si(a->rat, 1, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        f = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = log(f);
    }
}

static void minim_number_pow(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (minim_number_zerop(a)) // pow(0, -x) = +inf, pow(0, x) = 0
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = ((minim_number_negativep(b)) ? INFINITY : 0);
    }
    // pow(-inf, x), pow (inf, x), 
    else if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT &&
            !minim_number_negativep(b) && mpz_cmp_ui(&b->rat->_mp_den, 1) == 0 &&
            mpz_cmp_ui(&b->rat->_mp_num, 8192) < 0)
    {
        unsigned long e = mpz_get_ui(&b->rat->_mp_num);

        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpz_pow_ui(&res->rat->_mp_num, &a->rat->_mp_num, e);
        mpz_pow_ui(&res->rat->_mp_den, &a->rat->_mp_den, e);
    }
    else
    {
        double fa = ((a->type == MINIM_NUMBER_EXACT) ? mpq_get_d(a->rat) : a->fl);
        double fb = ((b->type == MINIM_NUMBER_EXACT) ? mpq_get_d(b->rat) : b->fl);

        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = pow(fa, fb);
    }
}

// *** Builtins *** //

void env_load_module_math(MinimEnv *env)
{
    env_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    env_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_sub);
    env_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_mul);
    env_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);

    env_load_builtin(env, "sqrt", MINIM_OBJ_FUNC, minim_builtin_sqrt);
    env_load_builtin(env, "exp", MINIM_OBJ_FUNC, minim_builtin_exp);
    env_load_builtin(env, "log", MINIM_OBJ_FUNC, minim_builtin_log);
    env_load_builtin(env, "pow", MINIM_OBJ_FUNC, minim_builtin_pow);
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

MinimObject *minim_builtin_sqrt(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(argc, args, &res, "sqrt", 1) &&
        assert_number(args[0], &res, "Expected a number for 'sqrt'"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_sqrt(num, args[0]->data);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_exp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(argc, args, &res, "exp", 1) &&
        assert_number(args[0], &res, "Expected a number for 'exp'"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_exp(num, args[0]->data);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_log(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(argc, args, &res, "log", 1) &&
        assert_number(args[0], &res, "Expected a number for 'log'"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_log(num, args[0]->data);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_pow(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(argc, args, &res, "pow", 2) &&
        assert_number(args[0], &res, "Expected a number in the 1st argument of 'pow'") &&
        assert_number(args[1], &res, "Expected a number in the 2nd argument of 'pow'"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_pow(num, args[0]->data, args[1]->data);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    free_minim_objects(argc, args);
    return res;
}
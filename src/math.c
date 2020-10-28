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

    if (minim_number_zerop(b))
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = NAN;
    }
    else if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
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
        if (mpz_cmpabs_ui(mpq_denref(a->rat), 1) == 0)
        {
            mpz_t sq, rem;

            mpz_inits(sq, rem, NULL);
            mpz_sqrtrem(sq, rem, mpq_numref(a->rat));   
            if (mpz_cmp_ui(rem, 0) == 0)
            {
                reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
                mpq_set_z(res->rat, sq);
            }
            else
            {
                reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
                res->fl = sqrt(mpq_get_d(a->rat));
            }

            mpz_clears(sq, rem, NULL);
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
    if (minim_number_zerop(b))  // pow(x, 0) = 0
    {
        if (minim_number_exactp(a) && minim_number_exactp(b))
        {
            reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
            mpq_set_ui(res->rat, 1, 1);
        }
        else
        {
            reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
            res->fl = 1.0;
        }
    }
    else if (minim_number_zerop(a)) // pow(0, -x) = +inf, pow(0, x) = 0
    {
        if (minim_number_negativep(b))
        {
            reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
            res->fl = INFINITY;
        }
        else if (minim_number_exactp(a) && minim_number_exactp(b))
        {
            reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
            mpq_set_ui(res->rat, 0, 1);
        }
        else
        {
            reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
            res->fl = 0.0;;
        }
    }
    // pow(-inf, x), pow (inf, x), 
    else if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT &&
            mpz_cmp_ui(&b->rat->_mp_den, 1) == 0)
    {
        unsigned long e;

        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        if (minim_number_negativep(b))
        {
            e = - mpz_get_si(mpq_numref(b->rat));
            mpz_pow_ui(mpq_denref(res->rat), mpq_numref(a->rat), e);
            mpz_pow_ui(mpq_numref(res->rat), mpq_denref(a->rat), e);
        }
        else
        {
            e = mpz_get_ui(mpq_numref(b->rat));
            mpz_pow_ui(mpq_numref(res->rat), mpq_numref(a->rat), e);
            mpz_pow_ui(mpq_denref(res->rat), mpq_denref(a->rat), e);
        }
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

    if (assert_min_argc(argc, &res, "+", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '+'", minim_numberp))
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

    if (assert_min_argc(argc, &res, "-", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '-'", minim_numberp))
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

    if (assert_min_argc(argc, &res, "*", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '*'", minim_numberp))
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

    if (assert_exact_argc(argc, &res, "/", 2) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '/'", minim_numberp))
    {
        copy_minim_object(&res, args[0]);
        minim_number_div(res->data, res->data, args[1]->data);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_sqrt(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(argc, &res, "sqrt", 1) &&
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

    if (assert_exact_argc(argc, &res, "exp", 1) &&
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

    if (assert_exact_argc(argc, &res, "log", 1) &&
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

    if (assert_exact_argc(argc, &res, "pow", 2) &&
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
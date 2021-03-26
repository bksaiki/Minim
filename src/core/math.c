#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "error.h"
#include "number.h"

#define GET_FLOAT(obj) (MINIM_OBJ_EXACTP(obj) ? mpq_get_d(MINIM_EXACT(obj)) : MINIM_INEXACT(obj))

// Internals

static bool all_exact(size_t argc, MinimObject **args)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (!MINIM_OBJ_EXACTP(args[i]))
            return false;
    }

    return true;
}

static MinimObject *minim_add(size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (all_exact(argc, args))
    {
        OPT_MOVE_REF(res, args[0]);
        for (size_t i = 1; i < argc; ++i)
            mpq_add(MINIM_EXACT(res), MINIM_EXACT(res), MINIM_EXACT(args[i]));
    }
    else
    {   
        init_minim_object(&res, MINIM_OBJ_INEXACT, GET_FLOAT(args[0]));
        for (size_t i = 1; i < argc; ++i)
            MINIM_INEXACT(res) += MINIM_INEXACT(args[i]);
    }

    return res;
}

static MinimObject *minim_neg(MinimObject *x)
{
    MinimObject *res;

    if (MINIM_OBJ_EXACTP(x))
    {
        res = copy2_minim_object(x);
        mpq_neg(MINIM_EXACT(res), MINIM_EXACT(res));
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, -MINIM_INEXACT(x));
    }

    return res;
}

static MinimObject *minim_sub2(MinimObject *x, MinimObject *y)
{
    MinimObject *res;

    if (MINIM_OBJ_EXACTP(x) && MINIM_OBJ_EXACTP(y))
    {
        res = copy2_minim_object(x);
        mpq_sub(MINIM_EXACT(res), MINIM_EXACT(res), MINIM_EXACT(y));
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, MINIM_INEXACT(x) - MINIM_INEXACT(y));
    }

    return res;
}

static MinimObject *minim_sub(MinimObject *first, size_t restc, MinimObject **rest)
{
    MinimObject *res, *psum;

    psum = minim_add(restc, rest);
    res = minim_sub2(first, psum);
    free_minim_object(psum);

    return res;
}

static MinimObject *minim_mul(size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (all_exact(argc, args))
    {
        OPT_MOVE_REF(res, args[0]);
        for (size_t i = 1; i < argc; ++i)
            mpq_mul(MINIM_EXACT(res), MINIM_EXACT(res), MINIM_EXACT(args[i]));
    }
    else
    {   
        init_minim_object(&res, MINIM_OBJ_INEXACT, GET_FLOAT(args[0]));
        for (size_t i = 1; i < argc; ++i)
            MINIM_INEXACT(res) *= MINIM_INEXACT(args[i]);
    }

    return res;
}

static MinimObject *minim_reciprocal(MinimObject *x)
{
    MinimObject *res;

    if (MINIM_OBJ_EXACTP(x))
    {
        res = copy2_minim_object(x);
        mpq_inv(MINIM_EXACT(res), MINIM_EXACT(res));
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, 1.0 / MINIM_INEXACT(x));
    }

    return res;
}

static MinimObject *minim_div(size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (all_exact(argc, args))
    {
        OPT_MOVE_REF(res, args[0]);
        for (size_t i = 1; i < argc; ++i)
            mpq_div(MINIM_EXACT(res), MINIM_EXACT(res), MINIM_EXACT(args[i]));
    }
    else
    {   
        init_minim_object(&res, MINIM_OBJ_INEXACT, GET_FLOAT(args[0]));
        for (size_t i = 1; i < argc; ++i)
            MINIM_INEXACT(res) /= MINIM_INEXACT(args[i]);
    }

    return res;
}

// Assumes integer arguments
static void minim_number_modulo(MinimNumber *res, MinimNumber *quo, MinimNumber *div)
{
    mpq_t q, d;

    // Convert to integer
    reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
    RATIONALIZE(q, quo);
    RATIONALIZE(d, div);

    mpq_set_ui(res->rat, 0, 1);
    mpz_fdiv_r(mpq_numref(res->rat), mpq_numref(q), mpq_numref(d));

    FREE_IF_INEXACT(q, quo);
    FREE_IF_INEXACT(d, div);

    if (quo->type == MINIM_NUMBER_INEXACT || div->type == MINIM_NUMBER_INEXACT)
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
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

static void minim_number_sin(MinimNumber *res, MinimNumber *arg)
{
    // sin(0) = 0
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = sin(GET_FLOAT(arg));
    }
}

static void minim_number_cos(MinimNumber *res, MinimNumber *arg)
{
    // cos(0) = 1
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 1, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = cos(GET_FLOAT(arg));
    }
}

static void minim_number_tan(MinimNumber *res, MinimNumber *arg)
{
    // tan(0) = 0
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = tan(GET_FLOAT(arg));
    }
}

static void minim_number_asin(MinimNumber *res, MinimNumber *arg)
{
    // asin(0) = 0
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = asin(GET_FLOAT(arg));
    }
}

static void minim_number_acos(MinimNumber *res, MinimNumber *arg)
{
    // acos(1) = 0
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 1, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = acos(GET_FLOAT(arg));
    }
}

static void minim_number_atan(MinimNumber *res, MinimNumber *arg)
{
    // atan(0) = 0
    if (arg->type == MINIM_NUMBER_EXACT && mpq_cmp_ui(arg->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = atan(GET_FLOAT(arg));
    }
}

static void minim_number_atan2(MinimNumber *res, MinimNumber *y, MinimNumber *x)
{
    // atan(0) = 0
    if (minim_number_zerop(y) && minim_number_zerop(x))
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = NAN;
    }
    else if (y->type == MINIM_NUMBER_EXACT && x->type == MINIM_NUMBER_EXACT &&
             mpq_cmp_ui(y->rat, 0, 1) == 0)
    {
        reinterpret_minim_number(res, MINIM_NUMBER_EXACT);
        mpq_set_ui(res->rat, 0, 1);
    }
    else
    {
        reinterpret_minim_number(res, MINIM_NUMBER_INEXACT);
        res->fl = atan2(GET_FLOAT(y), GET_FLOAT(x));
    }
}

// *** Builtins *** //

MinimObject *minim_builtin_add(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "+"))
        return res;

    if (argc == 0)
    {
        mpq_ptr zero = malloc(sizeof(__mpq_struct));

        mpq_init(zero);
        mpq_set_ui(zero, 0, 1);
        init_minim_object(&res, MINIM_OBJ_EXACT, zero);
    }
    else if (argc == 1)
    {
        OPT_MOVE_REF(res, args[0]);
    }
    else
    {
        res = minim_add(argc, args);
    }

    return res;
}

MinimObject *minim_builtin_sub(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    if (!assert_numerical_args(args, argc, &res, "-"))
        return res;

    if (argc == 1)          return minim_neg(args[0]);
    else if (argc == 2)     return minim_sub2(args[0], args[1]);
    else                    return minim_sub(args[0], argc - 1, &args[1]);
}

MinimObject *minim_builtin_mul(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "*"))
        return res;

    if (argc == 0)
    {
        mpq_ptr one = malloc(sizeof(__mpq_struct));

        mpq_init(one);
        mpq_set_ui(one, 0, 1);
        init_minim_object(&res, MINIM_OBJ_EXACT, one);
    }
    else if (argc == 1)
    {
        OPT_MOVE_REF(res, args[0]);
    }
    else
    {
        res = minim_mul(argc, args);
    }

    return res;
}

MinimObject *minim_builtin_div(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "/"))
        return res;

    if (argc == 1)  return minim_reciprocal(args[0]);
    else            return minim_div(argc, args);
}

MinimObject *minim_builtin_abs(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "abs"))
        return res;
    
    if (minim_negativep(args[0]))
        return minim_neg(args[0]);
    
    OPT_MOVE_REF(res, args[0]);
    return res;
}

MinimObject *minim_builtin_max(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t max = 0;

    if (!assert_numerical_args(args, argc, &res, "max"))
        return res;
    
    res = args[max];
    for (size_t i = 1; i < argc; ++i)
    {
        if (minim_number_cmp(args[i], args[max]) > 0)
            max = i;
    }

    OPT_MOVE_REF(res, args[max]);
    return res;
}

MinimObject *minim_builtin_min(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t min = 0;

    if (!assert_numerical_args(args, argc, &res, "min"))
        return res;

    res = args[min];
    for (size_t i = 1; i < argc; ++i)
    {
        if (minim_number_cmp(args[i], args[min]) < 0)
            min = i;
    }

    OPT_MOVE_REF(res, args[min]);
    return res;
}

MinimObject *minim_builtin_modulo(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    mpq_ptr r;
    mpq_t q, d;

    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "mod", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "mod", 1, args[1]);

    r = mallo

    init_minim_number(&mod, MINIM_NUMBER_EXACT);
    minim_number_modulo(mod, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
    return res;
}

MinimObject *minim_builtin_sqrt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_numerical_args(args, argc, &res, "sqrt"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_sqrt(num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_exp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_numerical_args(args, argc, &res, "exp"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_exp(num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_log(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_numerical_args(args, argc, &res, "log"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_log(num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_pow(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_numerical_args(args, argc, &res, "pow"))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_pow(num, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_sin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "sin"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_sin(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_cos(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "cos"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_cos(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_tan(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "tan"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_tan(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_asin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "asin"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_asin(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_acos(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "asin"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_acos(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_atan(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_numerical_args(args, argc, &res, "atan"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    if (argc == 1)  minim_number_atan(num, args[0]->u.ptrs.p1);
    else            minim_number_atan2(num, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
    
    init_minim_object(&res, MINIM_OBJ_NUM, num);
    return res;
}
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "number.h"

#define RATIONALIZE(r, num)                     \
{                                               \
    if ((num)->type == MINIM_NUMBER_INEXACT)    \
    {                                           \
        mpq_init(r);                            \
        mpq_set_d(r, (num)->fl);                \
    }                                           \
    else                                        \
    {                                           \
        r[0] = (num)->rat[0];                   \
    }                                           \
}                                               

#define FREE_IF_INEXACT(r, num)                 \
{                                               \
    if ((num)->type == MINIM_NUMBER_INEXACT)    \
        mpq_clear(r);                           \
}

#define GET_FLOAT(num)  (((num)->type == MINIM_NUMBER_EXACT) ?  \
                            mpq_get_d(num->rat) :               \
                            num->fl)

// Internals

static void minim_number_set(MinimNumber *num, MinimNumber *src)
{
    reinterpret_minim_number(num, src->type);
    if (src->type == MINIM_NUMBER_EXACT)
        mpq_set(num->rat, src->rat);
    else
        num->fl = src->fl;
}

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

static void minim_number_abs(MinimNumber *res, MinimNumber *a)
{
    if (minim_number_negativep(a))
        minim_number_neg(res, a);
    else
        minim_number_set(res, a);
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
    MinimNumber *num;

    if (assert_min_argc(&res, "+", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '+'", minim_numberp))
    {
        copy_minim_number(&num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
        for (size_t i = 1; i < argc; ++i)
            minim_number_add(num, num, args[i]->u.ptrs.p1);
    }

    return res;
}

MinimObject *minim_builtin_sub(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_min_argc(&res, "-", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '-'", minim_numberp))
    {    
        copy_minim_number(&num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
        if (argc == 1)
        {
            minim_number_neg(num, num);
        }
        else
        {
            for (size_t i = 1; i < argc; ++i)
                minim_number_sub(num, num, args[i]->u.ptrs.p1);
        
        }
    }

    return res;
}

MinimObject *minim_builtin_mul(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_min_argc(&res, "*", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '*'", minim_numberp))
    {
        copy_minim_number(&num, args[0]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
        for (size_t i = 1; i < argc; ++i)
            minim_number_mul(num, num, args[i]->u.ptrs.p1);
    }

    return res;
}

MinimObject *minim_builtin_div(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(&res, "/", 2, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '/'", minim_numberp))
    {
        init_minim_number(&num, MINIM_NUMBER_INEXACT);
        minim_number_div(num, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_abs(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_exact_argc(&res, "abs", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'abs'"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_abs(num, args[0]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_max(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num, *max;

    if (!assert_min_argc(&res, "max", 1, argc))
        return res;

    for (size_t i = 0; i < argc; ++i)
    {
        if (!assert_number(args[i], &res, "Expected numerical arguments for 'max'"))
            return res;
    }

    max = args[0]->u.ptrs.p1;
    for (size_t i = 1; i < argc; ++i)
    {
        if (minim_number_cmp(args[i]->u.ptrs.p1, max) > 0)
            max = args[i]->u.ptrs.p1;
    }
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_set(num, max);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_min(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num, *min;

    if (!assert_min_argc(&res, "min", 1, argc))
        return res;

    for (size_t i = 0; i < argc; ++i)
    {
        if (!assert_number(args[i], &res, "Expected numerical arguments for 'min'"))
            return res;
    }

    min = args[0]->u.ptrs.p1;
    for (size_t i = 1; i < argc; ++i)
    {
        if (minim_number_cmp(args[i]->u.ptrs.p1, min) < 0)
            min = args[i]->u.ptrs.p1;
    }
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    minim_number_set(num, min);
    init_minim_object(&res, MINIM_OBJ_NUM, num);

    return res;
}

MinimObject *minim_builtin_modulo(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *mod;

    if (!assert_exact_argc(&res, "modulo", 2, argc) ||
        !assert_integer(args[0], &res, "Expected an integer in the 1st argument of 'modulo'") ||
        !assert_integer(args[1], &res, "Expected an integer in the 2nd argument of 'modulo'"))
        return res;

    init_minim_number(&mod, MINIM_NUMBER_EXACT);
    minim_number_modulo(mod, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
    init_minim_object(&res, MINIM_OBJ_NUM, mod);

    return res;
}

MinimObject *minim_builtin_sqrt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (assert_exact_argc(&res, "sqrt", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'sqrt'"))
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

    if (assert_exact_argc(&res, "exp", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'exp'"))
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

    if (assert_exact_argc(&res, "log", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'log'"))
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

    if (assert_exact_argc(&res, "pow", 2, argc) &&
        assert_number(args[0], &res, "Expected a number in the 1st argument of 'pow'") &&
        assert_number(args[1], &res, "Expected a number in the 2nd argument of 'pow'"))
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

    if (!assert_exact_argc(&res, "sin", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'sin'"))
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

    if (!assert_exact_argc(&res, "cos", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'cos'"))
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

    if (!assert_exact_argc(&res, "tan", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'tan'"))
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

    if (!assert_exact_argc(&res, "sin", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'sin'"))
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

    if (!assert_exact_argc(&res, "cos", 1, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'cos'"))
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

    if (!assert_range_argc(&res, "tan", 1, 2, argc) ||
        !assert_number(args[0], &res, "Expected a number for 'tan'"))
        return res;
    
    init_minim_number(&num, MINIM_NUMBER_INEXACT);
    if (argc == 1)  minim_number_atan(num, args[0]->u.ptrs.p1);
    else            minim_number_atan2(num, args[0]->u.ptrs.p1, args[1]->u.ptrs.p1);
    
    init_minim_object(&res, MINIM_OBJ_NUM, num);
    return res;
}
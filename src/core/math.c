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

#define RATIONALIZE(rat, obj)                   \
{                                               \
    if (MINIM_OBJ_EXACTP(obj))                  \
    {                                           \
        rat = MINIM_EXACT(obj);                 \
    }                                           \
    else                                        \
    {                                           \
        rat = malloc(sizeof(__mpq_struct));     \
        mpq_init(rat);                          \
        mpq_set_d(rat, MINIM_INEXACT(obj));     \
    }                                           \
}

#define FREE_IF_INEXACT(rat, obj)               \
{                                               \
    if (MINIM_OBJ_INEXACTP(obj))                \
    {                                           \
        mpq_clear(rat);                         \
        free(rat);                              \
    }                                           \
}

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
        res = copy2_minim_object(args[0]);
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
        mpq_ptr rat = malloc(sizeof(__mpq_struct));

        mpq_init(rat);
        mpq_neg(rat, MINIM_EXACT(x));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
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
        mpq_ptr rat = malloc(sizeof(__mpq_struct));

        mpq_init(rat);
        mpq_sub(rat, MINIM_EXACT(x), MINIM_EXACT(y));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
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
        res = copy2_minim_object(args[0]);
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
        mpq_ptr rat = malloc(sizeof(__mpq_struct));

        mpq_init(rat);
        mpq_inv(MINIM_EXACT(res), MINIM_EXACT(res));
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, 1.0 / MINIM_INEXACT(x));
    }

    return res;
}

static MinimObject *minim_div2(MinimObject *x, MinimObject *y)
{
    MinimObject *res;

    if (minim_zerop(y))
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
    }
    else if (MINIM_OBJ_EXACTP(x) && MINIM_OBJ_EXACTP(y))
    {
        mpq_ptr rat = malloc(sizeof(__mpq_struct));

        mpq_init(rat);
        mpq_div(rat, MINIM_EXACT(x), MINIM_EXACT(y));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, GET_FLOAT(x) / GET_FLOAT(y));
    }

    return res;
}

static MinimObject *minim_div(MinimObject *first, size_t restc, MinimObject **rest)
{
    MinimObject *res, *prod;

    prod = minim_mul(restc, rest);
    res = minim_div2(first, prod);
    free_minim_object(prod);

    return res;
}

static MinimObject *minim_exact_sqrt(MinimObject *x)
{
    MinimObject *res;

    if (minim_integerp(x) && mpz_perfect_square_p(mpq_numref(MINIM_EXACT(x))))
    {
        mpq_ptr rat = malloc(sizeof(__mpq_struct));

        mpq_init(rat);
        mpz_sqrt(mpq_numref(rat), mpq_numref(MINIM_EXACT(x)));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, sqrt(GET_FLOAT(x)));
    }

    return res;
}

static MinimObject *minim_atan(MinimObject *x)
{
    MinimObject *res;

    // atan(0) = 0
    if (MINIM_OBJ_EXACTP(x) && minim_zerop(x))
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, tan(GET_FLOAT(x)));

    return res;
}

static MinimObject *minim_atan2(MinimObject *y, MinimObject *x)
{
    MinimObject *res;

    // atan2(0, 0) = NAN, atan2(0, x) = 0
    if (MINIM_EXACT(x) && MINIM_EXACT(y) && minim_zerop(y))
    {
        if (minim_zerop(x))
            init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
        else
            res = int_to_minim_number(0);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, atan2(GET_FLOAT(y), GET_FLOAT(x)));
    }

    return res;
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
        mpq_set_ui(one, 1, 1);
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

    if (argc == 1)          return minim_reciprocal(args[0]);
    else if (argc == 2)     return minim_div2(args[0], args[1]);
    else                    return minim_div(args[0], argc - 1,  &args[1]);
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
    mpq_ptr q, d, r;

    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "mod", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "mod", 1, args[1]);

    RATIONALIZE(q, args[0]);
    RATIONALIZE(d, args[1]);

    r = malloc(sizeof(__mpq_struct));
    mpq_init(r);
    mpq_set_ui(r, 0, 1);
    mpz_fdiv_r(mpq_numref(r), mpq_numref(q), mpq_numref(d));

    FREE_IF_INEXACT(q, args[0]);
    FREE_IF_INEXACT(d, args[1]);

    if (MINIM_OBJ_EXACTP(args[0]) && MINIM_OBJ_EXACTP(args[1]))
        init_minim_object(&res, MINIM_OBJ_EXACT, r);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, mpq_get_d(r)); 

    return res;
}

MinimObject *minim_builtin_sqrt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "sqrt", 0, args[0]);

    if (minim_negativep(args[0]))
        init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
    else if (MINIM_OBJ_EXACTP(args[0]))
        res = minim_exact_sqrt(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, sqrt(MINIM_INEXACT(args[0])));

    return res;
}

MinimObject *minim_builtin_exp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "exp", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]) && minim_zerop(args[0]))
        res = int_to_minim_number(1);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, exp(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_log(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "log", 0, args[0]);

    if (!minim_positivep(args[0]))
        init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
    else if (MINIM_OBJ_EXACTP(args[0]) && mpq_cmp_ui(MINIM_EXACT(args[0]), 1, 1) == 0)
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, log(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_pow(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool exact;

    if (!assert_numerical_args(args, argc, &res, "pow"))    
        return res;

    exact = all_exact(argc, args);
    if (minim_zerop(args[1]))   // pow(x, 0) = 1
    {
        res = (exact) ? int_to_minim_number(1) :
                        float_to_minim_number(1.0);
    }
    else if (minim_zerop(args[0]))  // pow(0, -x) = +inf, pow(0, x) = 0
    {
        if (minim_negativep(args[1]))
            init_minim_object(&res, MINIM_OBJ_INEXACT, INFINITY);
        else
            res = (exact) ? int_to_minim_number(0) :
                            float_to_minim_number(0.0);
    }
    else if (exact && minim_integerp(args[1]))
    {
        mpq_ptr rat;
        long int e;
        
        rat = malloc(sizeof(__mpq_struct));
        mpq_init(rat);
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);

        e = abs(mpz_get_si(mpq_numref(MINIM_EXACT(args[1]))));
        if (minim_negativep(args[1]))
        {
            mpz_pow_ui(mpq_denref(rat), mpq_numref(MINIM_EXACT(args[0])), e);
            mpz_pow_ui(mpq_numref(rat), mpq_denref(MINIM_EXACT(args[0])), e);
        }
        else
        {
            mpz_pow_ui(mpq_numref(rat), mpq_numref(MINIM_EXACT(args[0])), e);
            mpz_pow_ui(mpq_denref(rat), mpq_denref(MINIM_EXACT(args[0])), e);
        }
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, pow(GET_FLOAT(args[0]), GET_FLOAT(args[1])));
    }

    return res;
}

MinimObject *minim_builtin_sin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "sin", 0, args[0]);
    
    // sin(0) = 0
    if (MINIM_OBJ_EXACTP(args[0]) && minim_zerop(args[0]))
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, sin(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_cos(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "cos", 0, args[0]);
    
    // cos(0) = 1
    if (MINIM_OBJ_EXACTP(args[0]) && minim_zerop(args[0]))
        res = int_to_minim_number(1);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, cos(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_tan(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "tan", 0, args[0]);
    
    // tan(0) = 0
    if (MINIM_OBJ_EXACTP(args[0]) && minim_zerop(args[0]))
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, tan(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_asin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "asin", 0, args[0]);

    // asin(0) = 0
    if (MINIM_OBJ_EXACTP(args[0]) && minim_zerop(args[0]))
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, asin(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_acos(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "acos", 0, args[0]);

    // acos(1) = 0
    if (MINIM_OBJ_EXACTP(args[0]) && mpq_cmp_si(MINIM_EXACT(args[0]), 1, 1) == 0)
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, acos(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_atan(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "atan"))
        return res;

    if (argc == 1)  return minim_atan(args[0]);
    else            return minim_atan2(args[0], args[1]);
}
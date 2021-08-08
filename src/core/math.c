#include <math.h>
#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "env.h"
#include "error.h"
#include "number.h"

#define GET_FLOAT(obj) (MINIM_OBJ_EXACTP(obj) ? mpq_get_d(MINIM_EXACTNUM(obj)) : MINIM_INEXACTNUM(obj))

#define RATIONALIZE(rat, obj)                   \
{                                               \
    if (MINIM_OBJ_EXACTP(obj))                  \
    {                                           \
        rat = MINIM_EXACTNUM(obj);                 \
    }                                           \
    else                                        \
    {                                           \
        rat = gc_alloc_mpq_ptr();               \
        mpq_set_d(rat, MINIM_INEXACTNUM(obj));     \
    }                                           \
}

#define RATIONALIZE_COPY(rat, obj)              \
{                                               \
    rat = gc_alloc_mpq_ptr();                   \
    if (MINIM_OBJ_EXACTP(obj))                  \
        mpq_set(rat, MINIM_EXACTNUM(obj));         \
    else                                        \
        mpq_set_d(rat, MINIM_INEXACTNUM(obj));     \
}

typedef void (*mpz_2ary)(mpz_ptr, mpz_srcptr, mpz_srcptr);

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
        res = minim_exactnum(MINIM_EXACTNUM(args[0]));
        for (size_t i = 1; i < argc; ++i)
            mpq_add(MINIM_EXACTNUM(res), MINIM_EXACTNUM(res), MINIM_EXACTNUM(args[i]));
    }
    else
    {   
        res = minim_inexactnum(MINIM_INEXACTNUM(args[0]));
        for (size_t i = 1; i < argc; ++i)
            MINIM_INEXACTNUM(res) += MINIM_INEXACTNUM(args[i]);
    }

    return res;
}

static MinimObject *minim_neg(MinimObject *x)
{
    if (MINIM_OBJ_EXACTP(x))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpq_neg(rat, MINIM_EXACTNUM(x));
        return minim_exactnum(rat);
    }
    else
    {
        return minim_inexactnum(-MINIM_INEXACTNUM(x));
    }
}

static MinimObject *minim_sub2(MinimObject *x, MinimObject *y)
{
    if (MINIM_OBJ_EXACTP(x) && MINIM_OBJ_EXACTP(y))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpq_sub(rat, MINIM_EXACTNUM(x), MINIM_EXACTNUM(y));
        return minim_exactnum(rat);
    }
    else
    {
        return minim_inexactnum(MINIM_INEXACTNUM(x) - MINIM_INEXACTNUM(y));
    }
}

static MinimObject *minim_sub(MinimObject *first, size_t restc, MinimObject **rest)
{
    MinimObject *psum;

    psum = minim_add(restc, rest);
    return minim_sub2(first, psum);
}

static MinimObject *minim_mul(size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (all_exact(argc, args))
    {
        res = minim_exactnum(MINIM_EXACTNUM(args[0]));
        for (size_t i = 1; i < argc; ++i)
            mpq_mul(MINIM_EXACTNUM(res), MINIM_EXACTNUM(res), MINIM_EXACTNUM(args[i]));
    }
    else
    {   
        res = minim_inexactnum(MINIM_INEXACTNUM(args[0]));
        for (size_t i = 1; i < argc; ++i)
            MINIM_INEXACTNUM(res) *= MINIM_INEXACTNUM(args[i]);
    }

    return res;
}

static MinimObject *minim_reciprocal(MinimObject *x)
{
    MinimObject *res;

    if (MINIM_OBJ_EXACTP(x))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();
        mpq_inv(rat, MINIM_EXACTNUM(x));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, 1.0 / MINIM_INEXACTNUM(x));
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
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpq_div(rat, MINIM_EXACTNUM(x), MINIM_EXACTNUM(y));
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
    MinimObject *prod;

    prod = minim_mul(restc, rest);
    return minim_div2(first, prod);
}

static MinimObject *minim_exact_round_c11(MinimObject *x)
{
    MinimObject *res;
    mpq_ptr r;
    mpq_t a;

    if (minim_zerop(x))
    {
        return int_to_minim_number(0);
    }
    
    if (minim_integerp(x))
    {
        copy_minim_object(&res, x);
        return res;
    }

    r = gc_alloc_mpq_ptr();
    mpq_init(a);
    mpq_abs(a, MINIM_EXACTNUM(x));

    if (mpz_cmp_ui(mpq_denref(a), 2) == 0) // a = d / 2
    {
        // C11 says round ties away from 0 (abs ceiling)
        mpz_set_ui(mpq_denref(r), 1);
        mpz_cdiv_q(mpq_numref(r), mpq_numref(a), mpq_denref(a));
    }
    else
    {
        mpz_t q, m;

        mpz_inits(q, m, NULL);
        mpz_tdiv_qr(q, m, mpq_numref(a), mpq_denref(a));

        mpz_mul_ui(m, m, 2);
        if (mpz_cmp(m, mpq_denref(a)) > 0)
            mpz_add_ui(q, q, 1);

        mpq_set_z(r, q);
        mpz_clears(q, m, NULL);
    }

    mpq_clear(a);
    if (minim_negativep(x))
        mpq_neg(r, r);

    init_minim_object(&res, MINIM_OBJ_EXACT, r);
    return res;
}

static MinimObject *minim_int_2ary(MinimObject *x, MinimObject *y, mpz_2ary fun)
{
    MinimObject *res;
    mpq_ptr q, d, r;

    RATIONALIZE(q, x);
    RATIONALIZE(d, y);

    r = gc_alloc_mpq_ptr();

    mpq_set_ui(r, 0, 1);
    fun(mpq_numref(r), mpq_numref(q), mpq_numref(d));

    if (MINIM_OBJ_EXACTP(x) && MINIM_OBJ_EXACTP(y))
        init_minim_object(&res, MINIM_OBJ_EXACT, r);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, mpq_get_d(r));
    
    return res;
}

static MinimObject *minim_exact_sqrt(MinimObject *x)
{
    MinimObject *res;

    if (minim_integerp(x) && mpz_perfect_square_p(mpq_numref(MINIM_EXACTNUM(x))))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpz_sqrt(mpq_numref(rat), mpq_numref(MINIM_EXACTNUM(x)));
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
    if (MINIM_EXACTNUM(x) && MINIM_EXACTNUM(y) && minim_zerop(y))
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

MinimObject *minim_builtin_add(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "+"))
        return res;

    if (argc == 0)
    {
        mpq_ptr zero = gc_alloc_mpq_ptr();

        mpq_set_ui(zero, 0, 1);
        init_minim_object(&res, MINIM_OBJ_EXACT, zero);
    }
    else if (argc == 1)
    {
        return args[0];
    }
    else
    {
        return minim_add(argc, args);
    }

    return res;
}

MinimObject *minim_builtin_sub(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    
    if (!assert_numerical_args(args, argc, &res, "-"))
        return res;

    if (argc == 1)          return minim_neg(args[0]);
    else if (argc == 2)     return minim_sub2(args[0], args[1]);
    else                    return minim_sub(args[0], argc - 1, &args[1]);
}

MinimObject *minim_builtin_mul(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "*"))
        return res;

    if (argc == 0)
    {
        mpq_ptr one = gc_alloc_mpq_ptr();

        mpq_set_ui(one, 1, 1);
        init_minim_object(&res, MINIM_OBJ_EXACT, one);
    }
    else if (argc == 1)
    {
        return args[0];
    }
    else
    {
        return minim_mul(argc, args);
    }

    return res;
}

MinimObject *minim_builtin_div(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "/"))
        return res;

    if (argc == 1)          return minim_reciprocal(args[0]);
    else if (argc == 2)     return minim_div2(args[0], args[1]);
    else                    return minim_div(args[0], argc - 1,  &args[1]);
}

MinimObject *minim_builtin_abs(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "abs"))
        return res;
    
    if (minim_negativep(args[0]))
        return minim_neg(args[0]);
    
    return args[0];
}

MinimObject *minim_builtin_max(MinimEnv *env, size_t argc, MinimObject **args)
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

    return args[max];
}

MinimObject *minim_builtin_min(MinimEnv *env, size_t argc, MinimObject **args)
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

    return args[min];
}

MinimObject *minim_builtin_modulo(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "modulo", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "modulo", 1, args[1]);

    return minim_int_2ary(args[0], args[1], mpz_fdiv_r);
}

MinimObject *minim_builtin_remainder(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "remainder", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "remainder", 1, args[1]);

    return minim_int_2ary(args[0], args[1], mpz_tdiv_r);
}

MinimObject *minim_builtin_quotient(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "quotient", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "quotient", 1, args[1]);

    return minim_int_2ary(args[0], args[1], mpz_tdiv_q);
}

MinimObject *minim_builtin_numerator(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    mpq_ptr r;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "numerator", 0, args[0]);

    RATIONALIZE_COPY(r, args[0]);
    mpz_set_ui(mpq_denref(r), 1);
    if (MINIM_OBJ_EXACTP(args[0]))
    {
        init_minim_object(&res, MINIM_OBJ_EXACT, r);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, mpq_get_d(r));
    }

    return res;
}

MinimObject *minim_builtin_denominator(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    mpq_ptr r;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "denominator", 0, args[0]);

    RATIONALIZE_COPY(r, args[0]);
    mpz_set(mpq_numref(r), mpq_denref(r));
    mpz_set_ui(mpq_denref(r), 1);
    if (MINIM_OBJ_EXACTP(args[0]))
    {
        init_minim_object(&res, MINIM_OBJ_EXACT, r);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, mpq_get_d(r));
    }

    return res;
}

MinimObject *minim_builtin_floor(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "floor", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpz_set_ui(mpq_denref(rat), 1);
        mpz_fdiv_q(mpq_numref(rat), mpq_numref(MINIM_EXACTNUM(args[0])),
                                    mpq_denref(MINIM_EXACTNUM(args[0])));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, floor(MINIM_INEXACTNUM(args[0])));
    }

    return res;
}

MinimObject *minim_builtin_ceil(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "ceil", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpz_set_ui(mpq_denref(rat), 1);
        mpz_cdiv_q(mpq_numref(rat), mpq_numref(MINIM_EXACTNUM(args[0])),
                                    mpq_denref(MINIM_EXACTNUM(args[0])));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, ceil(MINIM_INEXACTNUM(args[0])));
    }

    return res;
}

MinimObject *minim_builtin_trunc(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "trunc", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpz_set_ui(mpq_denref(rat), 1);
        mpz_tdiv_q(mpq_numref(rat), mpq_numref(MINIM_EXACTNUM(args[0])),
                                    mpq_denref(MINIM_EXACTNUM(args[0])));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, trunc(MINIM_INEXACTNUM(args[0])));
    }

    return res;
}

MinimObject *minim_builtin_round(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "round", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]))
        res = minim_exact_round_c11(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, round(MINIM_INEXACTNUM(args[0])));

    return res;
}

MinimObject *minim_builtin_gcd(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "gcd", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "gcd", 1, args[1]);

    return minim_int_2ary(args[0], args[1], mpz_gcd);
}

MinimObject *minim_builtin_lcm(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "lcm", 0, args[0]);

    if (!minim_integerp(args[1]))
        return minim_argument_error("integer", "lcm", 1, args[1]);

    return minim_int_2ary(args[0], args[1], mpz_lcm);
}

MinimObject *minim_builtin_sqrt(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "sqrt", 0, args[0]);

    if (minim_negativep(args[0]))
        init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
    else if (MINIM_OBJ_EXACTP(args[0]))
        res = minim_exact_sqrt(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, sqrt(MINIM_INEXACTNUM(args[0])));

    return res;
}

MinimObject *minim_builtin_exp(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_log(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "log", 0, args[0]);

    if (!minim_positivep(args[0]))
        init_minim_object(&res, MINIM_OBJ_INEXACT, NAN);
    else if (MINIM_OBJ_EXACTP(args[0]) && mpq_cmp_ui(MINIM_EXACTNUM(args[0]), 1, 1) == 0)
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, log(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_pow(MinimEnv *env, size_t argc, MinimObject **args)
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
        
        rat = gc_alloc_mpq_ptr();
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);

        e = abs(mpz_get_si(mpq_numref(MINIM_EXACTNUM(args[1]))));
        if (minim_negativep(args[1]))
        {
            mpz_pow_ui(mpq_denref(rat), mpq_numref(MINIM_EXACTNUM(args[0])), e);
            mpz_pow_ui(mpq_numref(rat), mpq_denref(MINIM_EXACTNUM(args[0])), e);
        }
        else
        {
            mpz_pow_ui(mpq_numref(rat), mpq_numref(MINIM_EXACTNUM(args[0])), e);
            mpz_pow_ui(mpq_denref(rat), mpq_denref(MINIM_EXACTNUM(args[0])), e);
        }
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, pow(GET_FLOAT(args[0]), GET_FLOAT(args[1])));
    }

    return res;
}

MinimObject *minim_builtin_sin(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_cos(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_tan(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_asin(MinimEnv *env, size_t argc, MinimObject **args)
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

MinimObject *minim_builtin_acos(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "acos", 0, args[0]);

    // acos(1) = 0
    if (MINIM_OBJ_EXACTP(args[0]) && mpq_cmp_si(MINIM_EXACTNUM(args[0]), 1, 1) == 0)
        res = int_to_minim_number(0);
    else
        init_minim_object(&res, MINIM_OBJ_INEXACT, acos(GET_FLOAT(args[0])));

    return res;
}

MinimObject *minim_builtin_atan(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "atan"))
        return res;

    if (argc == 1)  return minim_atan(args[0]);
    else            return minim_atan2(args[0], args[1]);
}

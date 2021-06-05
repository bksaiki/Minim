#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "math.h"
#include "number.h"

// *** Internals *** //

static void GC_mpq_ptr_dtor(void *ptr) {
    mpq_clear((mpq_ptr) ptr);
}

mpq_ptr GC_alloc_mpq_ptr()
{
    mpq_ptr ptr = GC_alloc_opt(sizeof(__mpq_struct), GC_mpq_ptr_dtor, NULL);
    mpq_init(ptr);
    return ptr;
}

bool minim_zerop(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpz_cmp_ui(mpq_numref(MINIM_EXACT(num)), 0) == 0) :
           (MINIM_INEXACT(num) == 0.0);
}

bool minim_positivep(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpq_sgn(MINIM_EXACT(num)) > 0) :
           (MINIM_INEXACT(num) > 0.0);
}

bool minim_negativep(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpq_sgn(MINIM_EXACT(num)) < 0) :
           (MINIM_INEXACT(num) < 0.0);
}

bool minim_evenp(MinimObject *num)
{
    return MINIM_OBJ_EXACTP(num) ?
           mpz_even_p(mpq_numref(MINIM_EXACT(num))) :
           (fmod(MINIM_INEXACT(num), 2.0) == 0.0);
}

bool minim_oddp(MinimObject *num)
{
    return MINIM_OBJ_EXACTP(num) ?
           mpz_odd_p(mpq_numref(MINIM_EXACT(num))) :
           (fmod(MINIM_INEXACT(num), 2.0) == 1.0);
}

bool minim_integerp(MinimObject *thing)
{
    if (!MINIM_OBJ_NUMBERP(thing))
        return false;

    return (MINIM_OBJ_EXACTP(thing)) ?
           (mpz_cmp_ui(mpq_denref(MINIM_EXACT(thing)), 1) == 0) :
           (!isinf(MINIM_INEXACT(thing)) &&
                (ceil(MINIM_INEXACT(thing)) == MINIM_INEXACT(thing)));
}

bool minim_exact_integerp(MinimObject *thing)
{
    return MINIM_OBJ_EXACTP(thing) && mpz_cmp_ui(mpq_denref(MINIM_EXACT(thing)), 1) == 0;
}

bool minim_exact_nonneg_intp(MinimObject *thing)
{
    return MINIM_OBJ_EXACTP(thing) && mpq_sgn(MINIM_EXACT(thing)) >= 0 &&
           mpz_cmp_ui(mpq_denref(MINIM_EXACT(thing)), 1) == 0;
}

bool minim_nanp(MinimObject *thing)
{
    return MINIM_OBJ_INEXACTP(thing) && isnan(MINIM_INEXACT(thing));
}

bool minim_infinitep(MinimObject *thing)
{
    return MINIM_OBJ_INEXACTP(thing) && isinf(MINIM_INEXACT(thing));
}

MinimObject *int_to_minim_number(long int x)
{
    MinimObject *res;
    mpq_ptr rat = GC_alloc_mpq_ptr();

    mpq_set_si(rat, x, 1);
    init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    return res;
}

MinimObject *uint_to_minim_number(size_t x)
{
    MinimObject *res;
    mpq_ptr rat = GC_alloc_mpq_ptr();

    mpq_set_ui(rat, x, 1);
    init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    return res;
}

MinimObject *float_to_minim_number(double x)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_INEXACT, x);
    return res;
}

bool assert_numerical_args(MinimObject **args, size_t argc, MinimObject **res, const char *name)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (!MINIM_OBJ_NUMBERP(args[i]))
        {
            *res = minim_argument_error("number", name, i, args[i]);
            return false;
        }
    }

    return true;
}

int minim_number_cmp(MinimObject *a, MinimObject *b)
{
    int v;

    if (MINIM_OBJ_EXACTP(a) && MINIM_OBJ_EXACTP(b))
    {
        return mpq_cmp(MINIM_EXACT(a), MINIM_EXACT(b));
    }
    else if (MINIM_OBJ_EXACTP(a))
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, MINIM_INEXACT(b));
        v = mpq_cmp(MINIM_EXACT(a), r);
        mpq_clear(r);

        return v;
    }
    else if (MINIM_OBJ_EXACTP(b))
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, MINIM_INEXACT(a));
        v = mpq_cmp(r, MINIM_EXACT(b));
        mpq_clear(r);

        return v;
    }
    else
    {
        if (MINIM_INEXACT(a) > MINIM_INEXACT((b)))   return 1;
        if (MINIM_INEXACT(a) != MINIM_INEXACT((b)))  return -1;
        return 0;
    }
}

// *** Builtins *** //

MinimObject *minim_builtin_numberp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_NUMBERP(args[0]));
    return res;
}

MinimObject *minim_builtin_zerop(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "zero?", 0, args[0]);
    
    init_minim_object(&res, MINIM_OBJ_BOOL, minim_zerop(args[0]));
    return res;
}

MinimObject *minim_builtin_negativep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "negative?", 0, args[0]);
    
    init_minim_object(&res, MINIM_OBJ_BOOL, minim_negativep(args[0]));
    return res;
}

MinimObject *minim_builtin_positivep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "positive?", 0, args[0]);

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_positivep(args[0]));
    return res;
}

MinimObject *minim_builtin_evenp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "even?", 0, args[0]);
    
    init_minim_object(&res, MINIM_OBJ_BOOL, minim_evenp(args[0]));
    return res;
}

MinimObject *minim_builtin_oddp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!minim_integerp(args[0]))
        return minim_argument_error("integer", "odd?", 0, args[0]);

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_oddp(args[0]));
    return res;
}

MinimObject *minim_builtin_exactp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "exact?", 0, args[0]);

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_EXACTP(args[0]));
    return res;
}

MinimObject *minim_builtin_inexactp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "inexact?", 0, args[0]);

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_INEXACTP(args[0]));
    return res;
}

MinimObject *minim_builtin_integerp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_integerp(args[0]));
    return res;
}

MinimObject *minim_builtin_exact_integerp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_EXACTP(args[0]) && minim_integerp(args[0]));
    return res;
}

MinimObject *minim_builtin_nanp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_nanp(args[0]));
    return res;
}

MinimObject *minim_builtin_infinitep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_infinitep(args[0]));
    return res;
}

static bool minim_number_cmp_h(MinimObject **args, size_t argc, int op)
{
    int cmp;
    bool b;

    for (size_t i = 0; i < argc - 1; ++i)
    {
        cmp = minim_number_cmp(args[i], args[i + 1]);
        
        if (op == 0)        b = (cmp == 0);
        else if (op == 1)   b = (cmp > 0);
        else if (op == 2)   b = (cmp < 0);
        else if (op == 3)   b = (cmp >= 0);
        else if (op == 4)   b = (cmp <= 0);
        
        if (!b)     return false;
    }

    return true;
}

MinimObject *minim_builtin_eq(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "="))
        return res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 0));
    return res;
}

MinimObject *minim_builtin_gt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, ">"))
        return res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 1));
    return res;
}

MinimObject *minim_builtin_lt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "<"))
        return res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 2));
    return res;
}

MinimObject *minim_builtin_gte(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, ">="))
        return res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 3));
    return res;
}

MinimObject *minim_builtin_lte(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!assert_numerical_args(args, argc, &res, "<="))
        return res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 4));
    return res;
}

MinimObject *minim_builtin_to_exact(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "exact", 0, args[0]);

    if (MINIM_OBJ_EXACTP(args[0]))
    {
        OPT_MOVE_REF(res, args[0]);
    }
    else
    {
        mpq_ptr rat = GC_alloc_mpq_ptr();

        mpq_set_d(rat, MINIM_INEXACT(args[0]));
        init_minim_object(&res, MINIM_OBJ_EXACT, rat);
    }
    
    return res;
}

MinimObject *minim_builtin_to_inexact(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_NUMBERP(args[0]))
        return minim_argument_error("number", "inexact", 0, args[0]);

    if (MINIM_OBJ_INEXACTP(args[0]))
    {
        OPT_MOVE_REF(res, args[0]);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_INEXACT, mpq_get_d(MINIM_EXACT(args[0])));
    }

    return res;
}

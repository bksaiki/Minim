#include <math.h>

#include "minimpriv.h"

// *** Internals *** //

static void gc_mpq_ptr_dtor(void *ptr) {
    mpq_clear((mpq_ptr) ptr);
}

mpq_ptr gc_alloc_mpq_ptr()
{
    mpq_ptr ptr = GC_alloc_opt(sizeof(__mpq_struct), gc_mpq_ptr_dtor, NULL);
    mpq_init(ptr);
    return ptr;
}

bool minim_zerop(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpz_cmp_ui(mpq_numref(MINIM_EXACTNUM(num)), 0) == 0) :
           (MINIM_INEXACTNUM(num) == 0.0);
}

bool minim_positivep(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpq_sgn(MINIM_EXACTNUM(num)) > 0) :
           (MINIM_INEXACTNUM(num) > 0.0);
}

bool minim_negativep(MinimObject *num)
{
    return (MINIM_OBJ_EXACTP(num)) ?
           (mpq_sgn(MINIM_EXACTNUM(num)) < 0) :
           (MINIM_INEXACTNUM(num) < 0.0);
}

bool minim_evenp(MinimObject *num)
{
    return MINIM_OBJ_EXACTP(num) ?
           mpz_even_p(mpq_numref(MINIM_EXACTNUM(num))) :
           (fmod(MINIM_INEXACTNUM(num), 2.0) == 0.0);
}

bool minim_oddp(MinimObject *num)
{
    return MINIM_OBJ_EXACTP(num) ?
           mpz_odd_p(mpq_numref(MINIM_EXACTNUM(num))) :
           (fmod(MINIM_INEXACTNUM(num), 2.0) == 1.0);
}

bool minim_integerp(MinimObject *thing)
{
    if (!MINIM_OBJ_NUMBERP(thing))
        return false;

    return (MINIM_OBJ_EXACTP(thing)) ?
           (mpz_cmp_ui(mpq_denref(MINIM_EXACTNUM(thing)), 1) == 0) :
           (!isinf(MINIM_INEXACTNUM(thing)) &&
                (ceil(MINIM_INEXACTNUM(thing)) == MINIM_INEXACTNUM(thing)));
}

bool minim_exact_nonneg_intp(MinimObject *thing)
{
    return MINIM_OBJ_EXACTP(thing) && mpq_sgn(MINIM_EXACTNUM(thing)) >= 0 &&
           mpz_cmp_ui(mpq_denref(MINIM_EXACTNUM(thing)), 1) == 0;
}

bool minim_nanp(MinimObject *thing)
{
    return MINIM_OBJ_INEXACTP(thing) && isnan(MINIM_INEXACTNUM(thing));
}

bool minim_infinitep(MinimObject *thing)
{
    return MINIM_OBJ_INEXACTP(thing) && isinf(MINIM_INEXACTNUM(thing));
}

MinimObject *int_to_minim_number(long int x)
{
    mpq_ptr rat = gc_alloc_mpq_ptr();

    mpq_set_si(rat, x, 1);
    return minim_exactnum(rat);
}

MinimObject *uint_to_minim_number(size_t x)
{
    mpq_ptr rat = gc_alloc_mpq_ptr();

    mpq_set_ui(rat, x, 1);
    return minim_exactnum(rat);
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
        return mpq_cmp(MINIM_EXACTNUM(a), MINIM_EXACTNUM(b));
    }
    else if (MINIM_OBJ_EXACTP(a))
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, MINIM_INEXACTNUM(b));
        v = mpq_cmp(MINIM_EXACTNUM(a), r);
        mpq_clear(r);

        return v;
    }
    else if (MINIM_OBJ_EXACTP(b))
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, MINIM_INEXACTNUM(a));
        v = mpq_cmp(r, MINIM_EXACTNUM(b));
        mpq_clear(r);

        return v;
    }
    else
    {
        if (MINIM_INEXACTNUM(a) > MINIM_INEXACTNUM((b)))   return 1;
        if (MINIM_INEXACTNUM(a) != MINIM_INEXACTNUM((b)))  return -1;
        return 0;
    }
}

// *** Builtins *** //

MinimObject *minim_builtin_numberp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_NUMBERP(args[0]));
}

MinimObject *minim_builtin_zerop(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "zero?", 0, args[0]));
    
    return to_bool(minim_zerop(args[0]));
}

MinimObject *minim_builtin_negativep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "negative?", 0, args[0]));
    
    return to_bool(minim_negativep(args[0]));
}

MinimObject *minim_builtin_positivep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "positive?", 0, args[0]));

    return to_bool(minim_positivep(args[0]));
}

MinimObject *minim_builtin_evenp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        THROW(env, minim_argument_error("integer", "even?", 0, args[0]));
    
    return to_bool(minim_evenp(args[0]));
}

MinimObject *minim_builtin_oddp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_integerp(args[0]))
        THROW(env, minim_argument_error("integer", "odd?", 0, args[0]));

    return to_bool(minim_oddp(args[0]));
}

MinimObject *minim_builtin_exactp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "exact?", 0, args[0]));

    return to_bool(MINIM_OBJ_EXACTP(args[0]));
}

MinimObject *minim_builtin_inexactp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "inexact?", 0, args[0]));

    return to_bool(MINIM_OBJ_INEXACTP(args[0]));
}

MinimObject *minim_builtin_integerp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_integerp(args[0]));
}

MinimObject *minim_builtin_exact_integerp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_EXACTP(args[0]) && minim_integerp(args[0]));
}

MinimObject *minim_builtin_nanp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_nanp(args[0]));
}

MinimObject *minim_builtin_infinitep(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_infinitep(args[0]));
}

static bool minim_number_cmp_h(MinimObject **args, size_t argc, int op)
{
    int cmp;
    bool b;

    for (size_t i = 0; i < argc - 1; ++i)
    {
        cmp = minim_number_cmp(args[i], args[i + 1]);
        
        switch (op) {
        case 0:
            b = (cmp == 0);
            break;
        case 1:
            b = (cmp > 0);
            break;
        case 2:
            b = (cmp < 0);
            break;
        case 3:
            b = (cmp >= 0);
            break;
        case 4:
            b = (cmp <= 0);
            break;
        default:
            return false;
        }

        if (!b)
            return false;
    }

    return true;
}

MinimObject *minim_builtin_eq(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *err;

    if (!assert_numerical_args(args, argc, &err, "="))
        THROW(env, err);

    return to_bool(minim_number_cmp_h(args, argc, 0));
}

MinimObject *minim_builtin_gt(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *err;

    if (!assert_numerical_args(args, argc, &err, ">"))
        THROW(env, err);

    return to_bool(minim_number_cmp_h(args, argc, 1));
}

MinimObject *minim_builtin_lt(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *err;

    if (!assert_numerical_args(args, argc, &err, "<"))
        THROW(env, err);

    return to_bool(minim_number_cmp_h(args, argc, 2));
}

MinimObject *minim_builtin_gte(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *err;

    if (!assert_numerical_args(args, argc, &err, ">="))
        THROW(env, err);

    return to_bool(minim_number_cmp_h(args, argc, 3));
}

MinimObject *minim_builtin_lte(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *err;

    if (!assert_numerical_args(args, argc, &err, "<="))
        THROW(env, err);

    return to_bool(minim_number_cmp_h(args, argc, 4));
}

MinimObject *minim_builtin_to_exact(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "exact", 0, args[0]));

    if (MINIM_OBJ_EXACTP(args[0]))      // no change
        return args[0];

    mpq_ptr rat = gc_alloc_mpq_ptr();
    mpq_set_d(rat, MINIM_INEXACTNUM(args[0]));
    return minim_exactnum(rat);
}

MinimObject *minim_builtin_to_inexact(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_NUMBERP(args[0]))
        THROW(env, minim_argument_error("number", "inexact", 0, args[0]));

    if (MINIM_OBJ_INEXACTP(args[0]))    // no change
        return args[0];
    
    return minim_inexactnum(mpq_get_d(MINIM_EXACTNUM(args[0])));
}

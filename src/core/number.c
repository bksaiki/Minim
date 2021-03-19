#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "math.h"
#include "number.h"

// Visible functions

void init_minim_number(MinimNumber **pnum, MinimNumberType type)
{
    MinimNumber *num = malloc(sizeof(MinimNumber));

    num->type = type;
    if (type == MINIM_NUMBER_EXACT)     mpq_init(num->rat); 
    else                                num->fl = 0.0;                          

    *pnum = num;
}

void str_to_minim_number(MinimNumber* num, const char *str)
{
    if (num->type == MINIM_NUMBER_EXACT)     
    {
        mpq_set_str(num->rat, str, 0);
        mpq_canonicalize(num->rat);
    }
    else
    {
        num->fl = strtod(str, NULL);
    }
}

void reinterpret_minim_number(MinimNumber *num, MinimNumberType type)
{
    if (num->type != type)
    {
        if (num->type == MINIM_NUMBER_EXACT)
        {
            double d = mpq_get_d(num->rat);

            mpq_clear(num->rat);
            num->type = MINIM_NUMBER_INEXACT;
            num->fl = d;
        }
        else
        {
            double d = num->fl;

            num->type = MINIM_NUMBER_EXACT;
            mpq_init(num->rat);
            mpq_set_d(num->rat, d);
        }
    }
}

void copy_minim_number(MinimNumber **pnum, MinimNumber *src)
{
    MinimNumber *num = malloc(sizeof(MinimNumber));

    num->type = src->type;
    if (num->type == MINIM_NUMBER_EXACT)
    {
        mpq_init(num->rat);
        mpq_set(num->rat, src->rat);
    }
    else
    {
        num->fl = src->fl;
    }

    *pnum = num;
}

void free_minim_number(MinimNumber *num)
{
    if (num->type == MINIM_NUMBER_EXACT)    mpq_clear(num->rat);
    free(num);
}

char *minim_number_to_str(MinimNumber *num)
{
    char* str;
    size_t len = 128;

    if (num->type == MINIM_NUMBER_EXACT)
    {
        str = malloc(len * sizeof(char));
        len = gmp_snprintf(str, len, "%Qd", num->rat);

        if (len > 128)
        {
            str = realloc(str, (len + 1) * sizeof(char));
            gmp_snprintf(str, len, "%Qd", num->rat);
        }
    }
    else
    {
        bool dot = false;

        str = malloc(30 * sizeof(char));
        snprintf(str, 29, "%.16g", num->fl);
        
        for (char *it = str; *it; ++it)
        {
            if (*it == '.' || *it == 'e')
                dot = true;
        }

        if (!dot && strcmp(str, "inf") != 0 && strcmp(str, "-inf") != 0 && strcmp(str, "nan") != 0)
            strcat(str, ".0");
    }

    return str;
}

int minim_number_cmp(MinimNumber *a, MinimNumber *b)
{
    int v;

    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        return mpq_cmp(a->rat, b->rat);
    }
    else if (a->type == MINIM_NUMBER_EXACT)
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, b->fl);
        v = mpq_cmp(a->rat, r);

        mpq_clear(r);
        return v;
    }
    else if (b->type == MINIM_NUMBER_EXACT)
    {
        mpq_t r;

        mpq_init(r);
        mpq_set_d(r, a->fl);
        v = mpq_cmp(r, b->rat);

        mpq_clear(r);
        return v;
    }
    else
    {
        if (a->fl > b->fl)  return 1;
        if (a->fl < b->fl)  return -1;
        return 0;
    }
}

bool minim_number_zerop(MinimNumber *num)
{
    return ((num->type == MINIM_NUMBER_EXACT) ? mpq_cmp_si(num->rat, 0, 1) == 0 : num->fl == 0.0);
}

bool minim_number_negativep(MinimNumber *num)
{
    return ((num->type == MINIM_NUMBER_EXACT) ? mpq_cmp_si(num->rat, 0, 1) < 0 : num->fl < 0.0);
}

bool minim_number_positivep(MinimNumber *num)
{
    return ((num->type == MINIM_NUMBER_EXACT) ? mpq_cmp_si(num->rat, 0, 1) > 0 : num->fl > 0.0);
}

bool minim_number_integerp(MinimNumber *num)
{
    if (num->type == MINIM_NUMBER_EXACT)
    {
        return mpz_cmp_ui(mpq_denref(num->rat), 1) == 0;
    }
    else
    {
        if (isinf(num->fl))     return false;
        else                    return (ceil(num->fl) == num->fl);
    }
}

bool minim_number_exactp(MinimNumber *num)
{
    return num->type == MINIM_NUMBER_EXACT;
}

bool minim_number_inexactp(MinimNumber *num)
{
    return num->type == MINIM_NUMBER_INEXACT;
}

bool minim_number_exactintp(MinimNumber *num)
{
    return (num->type == MINIM_NUMBER_EXACT && mpz_cmp_ui(mpq_denref(num->rat), 1) == 0);
}

bool minim_number_exact_nonneg_intp(MinimNumber *num)
{
    return (num->type == MINIM_NUMBER_EXACT && mpz_cmp_ui(mpq_denref(num->rat), 1) == 0 && mpq_sgn(num->rat) >= 0);
}

bool assert_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM);
}

bool assert_integer(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_integerp(arg->u.ptrs.p1));
}

bool assert_exact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exactp(arg->u.ptrs.p1));
}

bool assert_inexact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_inexactp(arg->u.ptrs.p1));
}

bool assert_exact_int(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exactintp(arg->u.ptrs.p1));
}

bool assert_exact_nonneg_int(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exact_nonneg_intp(arg->u.ptrs.p1));
}

// *** Internals *** //

bool minim_exactp(MinimObject *thing)
{
    return (MINIM_OBJ_NUMBERP(thing) && ((MinimNumber*) thing->u.ptrs.p1)->type == MINIM_NUMBER_EXACT);
}

bool minim_inexactp(MinimObject *thing)
{
    return (MINIM_OBJ_NUMBERP(thing) && ((MinimNumber*) thing->u.ptrs.p1)->type == MINIM_NUMBER_INEXACT);
}

bool minim_integerp(MinimObject *thing)
{
    return MINIM_OBJ_NUMBERP(thing) && minim_number_integerp(MINIM_NUMBER(thing));
}

bool minim_exact_nonneg_intp(MinimObject *obj)
{
    MinimNumber *num;

    if (!MINIM_OBJ_NUMBERP(obj))
        return false;
        
    num = MINIM_NUMBER(obj);
    return MINIM_NUMBER_EXACTP(num) && mpq_sgn(num->rat) >= 0 && mpz_cmp_ui(mpq_denref(num->rat), 1) == 0;
}

void minim_number_to_bytes(MinimObject *obj, Buffer *bf)
{
    MinimNumber *num = obj->u.ptrs.p1;

    if (num->type == MINIM_NUMBER_EXACT)
    {
        // Dump integer limbs
        write_buffer(bf, num->rat->_mp_num._mp_d, abs(num->rat->_mp_num._mp_size) * sizeof(mp_limb_t));
        write_buffer(bf, num->rat->_mp_den._mp_d, abs(num->rat->_mp_den._mp_size) * sizeof(mp_limb_t));
    }
    else
    {
        write_buffer(bf, &num->fl, sizeof(double));
    }
}

size_t minim_number_to_uint(MinimObject *obj)
{
    MinimNumber *num = obj->u.ptrs.p1;

    if (num->type == MINIM_NUMBER_INEXACT)
    {
        printf("Can't convert inexact number to string\n");
        return 0;
    }
    else
    {
        return mpz_get_ui(mpq_numref(num->rat));
    }
}

MinimNumber *minim_number_pi()
{
    MinimNumber *pi;

    init_minim_number(&pi, MINIM_NUMBER_INEXACT);
    pi->fl = 3.141592653589793238465;

    return pi;
}

MinimNumber *minim_number_phi()
{
    MinimNumber *phi;

    init_minim_number(&phi, MINIM_NUMBER_INEXACT);
    phi->fl = 1.618033988749894848204;

    return phi;
}

MinimNumber *minim_number_inf()
{
    MinimNumber *pi;

    init_minim_number(&pi, MINIM_NUMBER_INEXACT);
    pi->fl = INFINITY;

    return pi;
}

MinimNumber *minim_number_ninf()
{
    MinimNumber *pi;

    init_minim_number(&pi, MINIM_NUMBER_INEXACT);
    pi->fl = -INFINITY;

    return pi;
}

MinimNumber *minim_number_nan()
{
    MinimNumber *phi;

    init_minim_number(&phi, MINIM_NUMBER_INEXACT);
    phi->fl = NAN;

    return phi;
}

// *** Builtins *** //

MinimObject *minim_builtin_numberp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "number?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_NUMBERP(args[0]));

    return res;
}

MinimObject *minim_builtin_zerop(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "zero?", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'zero?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_zerop(args[0]->u.ptrs.p1));
    }

    return res;
}

MinimObject *minim_builtin_negativep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "negative?", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'negative?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_negativep(args[0]->u.ptrs.p1));
    }

    return res;
}

MinimObject *minim_builtin_positivep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "positive?", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'positive?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_positivep(args[0]->u.ptrs.p1));
    }

    return res;
}

MinimObject *minim_builtin_exactp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "exact?", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'exact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_exactp(args[0]->u.ptrs.p1));
    }

    return res;
}

MinimObject *minim_builtin_inexactp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "inexact?", 1, argc) &&
        assert_number(args[0], &res, "Expected a number for 'inexact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_inexactp(args[0]->u.ptrs.p1));
    }

    return res;
}

MinimObject *minim_builtin_integerp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "integer?", 1, argc))
    {
        if (MINIM_OBJ_NUMBERP(args[0]))
            init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_integerp(args[0]->u.ptrs.p1));
        else
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
    }

    return res;
}

MinimObject *minim_builtin_exact_integerp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "exact-integer?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_NUMBERP(args[0]) && minim_number_exactintp(args[0]->u.ptrs.p1));

    return res;
}

static bool minim_number_cmp_h(MinimObject **args, size_t argc, int op)
{
    int cmp;
    bool b;

    for (size_t i = 0; i < argc - 1; ++i)
    {
        cmp = minim_number_cmp(args[i]->u.ptrs.p1, args[i + 1]->u.ptrs.p1);
        
        if (op == 0)        b = (cmp == 0);
        else if (op == 1)   b = (cmp > 0);
        else if (op == 2)   b = (cmp < 0);
        else if (op == 3)   b = (cmp >= 0);
        else if (op == 4)   b = (cmp <= 0);
        
        if (!b)     return false;
    }

    return true;
}

bool minim_numberp(MinimObject *thing)
{
    return MINIM_OBJ_NUMBERP(thing);
}

MinimObject *minim_builtin_eq(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "=", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 0));
    }

    return res;
}

MinimObject *minim_builtin_gt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, ">", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '>'", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 1));
    }

    return res;
}

MinimObject *minim_builtin_lt(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "<", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '<'", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 2));
    }

    return res;
}

MinimObject *minim_builtin_gte(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, ">=", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '>='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 3));
    }

    return res;
}

MinimObject *minim_builtin_lte(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "<=", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected numerical arguments for '<='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(args, argc, 4));
    }

    return res;
}

MinimObject *minim_builtin_to_exact(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "exact", 1, argc) &&
        assert_number(args[0], &res, "Expected a number in the 1st argument of 'exact'"))
    {
        MinimNumber *num = args[0]->u.ptrs.p1;

        if (num->type == MINIM_NUMBER_EXACT)
        {
            OPT_MOVE_REF(res, args[0]);
        }
        else
        {
            MinimNumber *exact;

            init_minim_number(&exact, MINIM_NUMBER_EXACT);
            init_minim_object(&res, MINIM_OBJ_NUM, exact);
            mpq_set_d(exact->rat, num->fl);
        }
    }
    
    return res;
}

MinimObject *minim_builtin_to_inexact(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "inexact", 1, argc) &&
        assert_number(args[0], &res, "Expected a number in the 1st argument of 'inexact'"))
    {
        MinimNumber *num = args[0]->u.ptrs.p1;

        if (num->type == MINIM_NUMBER_INEXACT)
        {
            OPT_MOVE_REF(res, args[0]);
        }
        else
        {
            MinimNumber *inexact;

            init_minim_number(&inexact, MINIM_NUMBER_INEXACT);
            init_minim_object(&res, MINIM_OBJ_NUM, inexact);
            inexact->fl = mpq_get_d(num->rat);
        }
    }

    return res;
}
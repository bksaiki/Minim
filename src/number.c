#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"
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
            mpq_clear(num->rat);
            num->type = MINIM_NUMBER_INEXACT;
        }
        else
        {
            mpq_init(num->rat);
            num->type = MINIM_NUMBER_EXACT;
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
    int len = 128;

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

        if (!dot) strcat(str, ".0");
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

bool assert_exact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exactp(arg->data));
}

bool assert_inexact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_inexactp(arg->data));
}

bool assert_exact_int(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exactintp(arg->data));
}

bool assert_exact_nonneg_int(MinimObject *arg, MinimObject **ret, const char *msg)
{
    return assert_generic(ret, msg, arg->type == MINIM_OBJ_NUM && minim_number_exact_nonneg_intp(arg->data));
}

// *** Internals *** //

bool minim_numberp(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_NUM;
}

bool minim_exactp(MinimObject *thing)
{
    return (thing->type == MINIM_OBJ_NUM &&
            ((MinimNumber*) thing->data)->type == MINIM_NUMBER_EXACT);
}

bool minim_inexactp(MinimObject *thing)
{
    return (thing->type == MINIM_OBJ_NUM &&
            ((MinimNumber*) thing->data)->type == MINIM_NUMBER_INEXACT);
}

void minim_number_to_bytes(MinimObject *obj, Buffer *bf)
{
    MinimNumber *num = obj->data;

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

// *** Builtins *** //

void env_load_module_number(MinimEnv *env)
{
    env_load_builtin(env, "number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    env_load_builtin(env, "zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
    env_load_builtin(env, "positive?", MINIM_OBJ_FUNC, minim_builtin_positivep);
    env_load_builtin(env, "negative?", MINIM_OBJ_FUNC, minim_builtin_negativep);
    env_load_builtin(env, "exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    env_load_builtin(env, "inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);

    env_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    env_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    env_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    env_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    env_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);
}

MinimObject *minim_builtin_numberp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "number?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_numberp(args[0]));

    return res;
}

MinimObject *minim_builtin_zerop(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "zero?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'zero?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_zerop(args[0]->data));
    }

    return res;
}

MinimObject *minim_builtin_negativep(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "negative?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'negative?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_negativep(args[0]->data));
    }

    return res;
}

MinimObject *minim_builtin_positivep(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "positive?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'positive?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_positivep(args[0]->data));
    }

    return res;
}

MinimObject *minim_builtin_exactp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "exact?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'exact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_exactp(args[0]->data));
    }

    return res;
}

MinimObject *minim_builtin_inexactp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "inexact?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'inexact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_inexactp(args[0]->data));
    }

    return res;
}

static bool minim_number_cmp_h(int argc, MinimObject **args, int op)
{
    int cmp;
    bool b;

    for (int i = 0; i < argc - 1; ++i)
    {
        cmp = minim_number_cmp(args[i]->data, args[i + 1]->data);
        
        if (op == 0)        b = (cmp == 0);
        else if (op == 1)   b = (cmp > 0);
        else if (op == 2)   b = (cmp < 0);
        else if (op == 3)   b = (cmp >= 0);
        else if (op == 4)   b = (cmp <= 0);
        
        if (!b)     return false;
    }

    return true;
}

MinimObject *minim_builtin_eq(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, "=", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(argc, args, 0));
    }

    return res;
}

MinimObject *minim_builtin_gt(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, ">", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '>'", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(argc, args, 1));
    }

    return res;
}

MinimObject *minim_builtin_lt(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, "<", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '<'", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(argc, args, 2));
    }

    return res;
}

MinimObject *minim_builtin_gte(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, ">=", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '>='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(argc, args, 3));
    }

    return res;
}

MinimObject *minim_builtin_lte(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, "<=", 1) &&
        assert_for_all(argc, args, &res, "Expected numerical arguments for '<='", minim_numberp))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_cmp_h(argc, args, 4));
    }

    return res;
}

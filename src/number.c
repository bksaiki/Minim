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
    if (num->type == MINIM_NUMBER_EXACT)     mpq_set_str(num->rat, str, 0);
    else                                     num->fl = strtod(str, NULL);
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
            str = malloc((len + 1) * sizeof(char));
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

bool assert_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (arg->type != MINIM_OBJ_NUM)
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_exact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!assert_number(arg, ret, msg) && !minim_number_exactp(arg->data))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_inexact_number(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!assert_number(arg, ret, msg) && !minim_number_inexactp(arg->data))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
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

// *** Builtins *** //

void env_load_module_number(MinimEnv *env)
{
    env_load_builtin(env, "number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    env_load_builtin(env, "zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
    env_load_builtin(env, "positive?", MINIM_OBJ_FUNC, minim_builtin_positivep);
    env_load_builtin(env, "negative?", MINIM_OBJ_FUNC, minim_builtin_negativep);
    env_load_builtin(env, "exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    env_load_builtin(env, "inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);
}

MinimObject *minim_builtin_numberp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "number?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_numberp(args[0]));
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_zerop(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "zero?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'zero?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_zerop(args[0]->data));
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_negativep(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "negative?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'negative?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_negativep(args[0]->data));
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_positivep(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "positive?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'positive?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_positivep(args[0]->data));
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_exactp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "exact?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'exact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_exactp(args[0]->data));
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_inexactp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "inexact?", 1) &&
        assert_number(args[0], &res, "Expected a number for 'inexact?'"))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_number_inexactp(args[0]->data));
    }

    free_minim_objects(argc, args);
    return res;
}
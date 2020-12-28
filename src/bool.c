#include "assert.h"
#include "bool.h"
#include "list.h"

//
// Visible functions
//

bool minim_boolp(MinimObject *thing)
{
    return (thing->type == MINIM_OBJ_BOOL);
}

bool coerce_into_bool(MinimObject *obj)
{
    if (obj->type == MINIM_OBJ_BOOL)
    {
        return obj->si;
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        return MINIM_CAR(obj) || MINIM_CDR(obj);
    }
    else
    {
        return true;
    }
}

void env_load_module_bool(MinimEnv *env)
{
    env_load_builtin(env, "true", MINIM_OBJ_BOOL, 1);
    env_load_builtin(env, "false", MINIM_OBJ_BOOL, 0);

    env_load_builtin(env, "bool?", MINIM_OBJ_FUNC, minim_builtin_boolp);
    env_load_builtin(env, "not", MINIM_OBJ_FUNC, minim_builtin_not);
    env_load_builtin(env, "or", MINIM_OBJ_FUNC, minim_builtin_or);
    env_load_builtin(env, "and", MINIM_OBJ_FUNC, minim_builtin_and);
}

MinimObject *minim_builtin_boolp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "bool?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_boolp(args[0]));
    
    return res;
}

MinimObject *minim_builtin_not(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "not", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, !coerce_into_bool(args[0]));

    return res;
}

MinimObject *minim_builtin_or(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    bool val = false;

    if (assert_min_argc(&res, "or", 1, argc))
    {
        for (int i = 0; i < argc; ++i)
        {
            if (coerce_into_bool(args[i]))
            {
                val = true;
                res = args[i];
                args[i] = NULL;
                break;
            }
        }

        if (!val)
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
    }

    return res;
}

MinimObject *minim_builtin_and(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    bool val = true;

    if (assert_min_argc(&res, "and", 1, argc))
    {
        for (int i = 0; i < argc; ++i)
        {
            if (!coerce_into_bool(args[i]))
            {
                val = false;
                break;
            }
        }

        if (val)
        {
            res = args[argc - 1];
            args[argc - 1] = NULL;
        }
        else
        {
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
        }
    }

    return res;
}
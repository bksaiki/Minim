#include "assert.h"
#include "bool.h"
#include "list.h"

//
// Visible functions
//

bool coerce_into_bool(MinimObject *obj)
{
    if (obj->type == MINIM_OBJ_BOOL)
    {
        return obj->u.ints.i1;
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

MinimObject *minim_builtin_boolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "bool?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_BOOLP(args[0]));
    
    return res;
}

MinimObject *minim_builtin_not(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "not", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, !coerce_into_bool(args[0]));

    return res;
}

MinimObject *minim_builtin_or(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool val = false;

    if (assert_min_argc(&res, "or", 1, argc))
    {
        for (size_t i = 0; i < argc; ++i)
        {
            if (coerce_into_bool(args[i]))
            {
                val = true;
                res = args[i];
                args[i] = NULL;
                break;
            }
        }

        init_minim_object(&res, MINIM_OBJ_BOOL, ((val) ? 1 : 0));
    }

    return res;
}

MinimObject *minim_builtin_and(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool val = true;

    if (assert_min_argc(&res, "and", 1, argc))
    {
        for (size_t i = 0; i < argc; ++i)
        {
            if (!coerce_into_bool(args[i]))
            {
                val = false;
                break;
            }
        }

        init_minim_object(&res, MINIM_OBJ_BOOL, ((val) ? 1 : 0));
    }

    return res;
}
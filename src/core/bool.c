#include "assert.h"
#include "list.h"

MinimObject *minim_builtin_boolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_BOOLP(args[0]));
    return res;
}

MinimObject *minim_builtin_not(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, !coerce_into_bool(args[0]));
    return res;
}

MinimObject *minim_builtin_or(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    for (size_t i = 0; i < argc; ++i)
    {
        if (coerce_into_bool(args[i]))
        {
            OPT_MOVE(res, args[i]);
            return res;
        }
    }

    init_minim_object(&res, MINIM_OBJ_BOOL, 0);
    return res;
}

MinimObject *minim_builtin_and(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    for (size_t i = 0; i < argc; ++i)
    {
        if (!coerce_into_bool(args[i]))
        {
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
            return res;
        }
    }

    OPT_MOVE(res, args[argc - 1]);
    return res;
}

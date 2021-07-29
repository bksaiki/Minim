#include "assert.h"
#include "list.h"

MinimObject *minim_builtin_boolp(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_BOOLP(args[0]));
    return res;
}

MinimObject *minim_builtin_not(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, !coerce_into_bool(args[0]));
    return res;
}

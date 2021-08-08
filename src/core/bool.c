#include "assert.h"
#include "list.h"

MinimObject *minim_builtin_boolp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_bool(MINIM_OBJ_BOOLP(args[0]));
}

MinimObject *minim_builtin_not(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_bool(!coerce_into_bool(args[0]));
}

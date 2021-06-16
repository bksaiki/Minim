#include "builtin.h"

MinimObject *minim_builtin_delay(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    init_minim_object(&res, MINIM_OBJ_PROMISE, args[0]);
    return res;
}

MinimObject *minim_builtin_force(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

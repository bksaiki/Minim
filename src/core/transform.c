#include "builtin.h"

MinimObject *minim_builtin_def_syntax(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;   
}

MinimObject *minim_builtin_syntax_rules(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

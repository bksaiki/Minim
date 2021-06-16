#include "builtin.h"
#include "error.h"
#include "eval.h"

MinimObject *minim_builtin_delay(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    init_minim_object(&res, MINIM_OBJ_PROMISE, args[0]);
    return res;
}

MinimObject *minim_builtin_force(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *val;
    
    if (!MINIM_OBJ_PROMISEP(args[0]))
        return minim_argument_error("force", "promise", 0, args[0]);

    if (MINIM_CDR(args[0]))
    {
        return MINIM_CAR(args[0]);  // already evaluated
    }
    else
    {
        eval_ast(env, MINIM_DATA(MINIM_CAR(args[0])), &val);
        MINIM_CAR(args[0]) = val;
        MINIM_CDR(args[0]) = (void*) 1;
        return val;
    }
}

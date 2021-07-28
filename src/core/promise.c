#include "builtin.h"
#include "error.h"
#include "eval.h"

MinimObject *minim_builtin_delay(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    MinimEnv *env2;
    
    rcopy_env(&env2, env);
    init_minim_object(&res, MINIM_OBJ_PROMISE, args[0], env2);
    return res;
}

MinimObject *minim_builtin_force(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_PROMISEP(args[0]))
        return minim_argument_error("force", "promise", 0, args[0]);

    if (MINIM_PROMISE_FORCEDP(args[0]))     // already evaluated
    {
        return MINIM_PROMISE_VAL(args[0]);
    }
    else
    {
        eval_ast_no_check(MINIM_PROMISE_ENV(args[0]),
                          MINIM_DATA(MINIM_PROMISE_VAL(args[0])),
                          &MINIM_PROMISE_VAL(args[0]));
        MINIM_PROMISE_SET_FORCED(args[0]);
        return MINIM_PROMISE_VAL(args[0]);
    }
}

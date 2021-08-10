#include "builtin.h"
#include "error.h"
#include "eval.h"

MinimObject *minim_builtin_delay(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimEnv *env2;
    
    rcopy_env(&env2, env);
    return minim_promise(args[0], env2);
}

MinimObject *minim_builtin_force(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_PROMISEP(args[0]))
        return minim_argument_error("force", "promise", 0, args[0]);

    if (MINIM_PROMISE_STATE(args[0]))     // already evaluated
    {
        return MINIM_PROMISE_VAL(args[0]);
    }
    else
    {
        eval_ast_no_check(MINIM_PROMISE_ENV(args[0]),
                          MINIM_AST(MINIM_PROMISE_VAL(args[0])),
                          &MINIM_PROMISE_VAL(args[0]));
        MINIM_PROMISE_SET_STATE(args[0], 1);
        return MINIM_PROMISE_VAL(args[0]);
    }
}

MinimObject *minim_builtin_promisep(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_PROMISEP(args[0]));
}

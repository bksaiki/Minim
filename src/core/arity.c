#include "minimpriv.h"

#define MAX_ARGC        32768
#define IS_OUTSIDE(x, min, max)         (x < min || x > max)
#define FULL_BUILTIN_NAME(partial)      minim_builtin_ ## partial
#define BUILTIN_EQUALP(fun, builtin)    (fun == FULL_BUILTIN_NAME(builtin))

#define SET_ARITY_RANGE(builtin, min, max)          \
{                                                   \
    if (BUILTIN_EQUALP(fun, builtin))               \
    {                                               \
        parity->low = min;                          \
        parity->high = max;                         \
        return true;                                \
    }                                               \
}

#define SET_ARITY_EXACT(builtin, exact)     SET_ARITY_RANGE(builtin, exact, exact)
#define SET_ARITY_MIN(builtin, min)         SET_ARITY_RANGE(builtin, min, SIZE_MAX)

bool minim_get_syntax_arity(MinimPrimClosureFn fun, MinimArity *parity)
{
    SET_ARITY_EXACT(def_values, 2);
    SET_ARITY_EXACT(setb, 2);
    SET_ARITY_EXACT(if, 3);
    SET_ARITY_MIN(let_values, 2);
    SET_ARITY_MIN(begin, 1);
    SET_ARITY_EXACT(quote, 1);
    SET_ARITY_EXACT(quasiquote, 1);
    SET_ARITY_EXACT(unquote, 1);
    SET_ARITY_MIN(lambda, 2);
    SET_ARITY_EXACT(delay, 1);
    SET_ARITY_EXACT(force, 1);
    SET_ARITY_EXACT(callcc, 1);

    SET_ARITY_EXACT(def_syntaxes, 2);
    SET_ARITY_MIN(syntax_case, 2)
    SET_ARITY_EXACT(syntax, 1);
    SET_ARITY_EXACT(template, 1);
    SET_ARITY_EXACT(identifier_equalp, 2);

    parity->low = 0;
    parity->high = SIZE_MAX;
    return true;
}

bool minim_get_lambda_arity(MinimLambda *lam, MinimArity *parity)
{
    parity->low = lam->argc;
    parity->high = (lam->rest) ? SIZE_MAX : parity->low;

    return true;
}

bool minim_check_prim_closure_arity(MinimObject *prim, size_t argc, MinimObject **perr)
{
    MinimArity arity;

    arity.low = MINIM_PRIM_CLOSURE_MIN_ARGC(prim);
    arity.high = MINIM_PRIM_CLOSURE_MAX_ARGC(prim);
    if (IS_OUTSIDE(argc, arity.low, arity.high))
    {
        *perr = minim_arity_error(MINIM_PRIM_CLOSURE_NAME(prim), arity.low, arity.high, argc);
        return false;
    }

    return true;
}

bool minim_check_syntax_arity(MinimPrimClosureFn fun, size_t argc, MinimEnv *env)
{
    MinimArity arity;

    if (!minim_get_syntax_arity(fun, &arity))
        return false;

    if (IS_OUTSIDE(argc, arity.low, arity.high))
        return false;

    return true;
}

// Builtins

MinimObject *minim_builtin_procedurep(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_FUNCP(args[0]));
}

MinimObject *minim_builtin_procedure_arity(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *min, *max;
    MinimArity arity;
    
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("procedure", "procedure-arity", 0, args[0]));

    if (MINIM_OBJ_PRIM_CLOSUREP(args[0]))
    {
        arity.low = MINIM_PRIM_CLOSURE_MIN_ARGC(args[0]);
        arity.high = MINIM_PRIM_CLOSURE_MAX_ARGC(args[0]);
    }
    else if (MINIM_OBJ_CLOSUREP(args[0]))
    {
        MinimLambda *lam = MINIM_CLOSURE(args[0]);

        arity.low = lam->argc;
        arity.high = (lam->rest) ? SIZE_MAX : arity.low;
    }
    else // MINIM_OBJ_JUMPP
    {
        arity.low = 0;
        arity.high = SIZE_MAX;
    }

    if (arity.low == arity.high)
    {
        res = uint_to_minim_number(arity.low);
    }
    else
    {
        min = uint_to_minim_number(arity.low);
        max = (arity.high == SIZE_MAX) ? to_bool(0) : uint_to_minim_number(arity.high);
        res = minim_cons(min, max);
    }

    return res;
}

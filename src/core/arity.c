#include "arity.h"
#include "error.h"
#include "lambda.h"
#include "number.h"

#define IS_OUTSIDE(x, min, max)         (x < min || x > max)
#define FULL_BUILTIN_NAME(partial)      minim_builtin_ ## partial
#define BUILTIN_EQUALP(fun, builtin)    (fun == FULL_BUILTIN_NAME(builtin))

static void builtin_arity_error(MinimBuiltin builtin, size_t argc, size_t min, size_t max,
                                MinimEnv *env, MinimObject **perr)
{
    MinimObject *obj;
    const char *name;

    init_minim_object(&obj, MINIM_OBJ_FUNC, builtin);
    name = env_peek_key(env, obj);
    *perr = minim_arity_error(name, min, max, argc);
}

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

bool minim_get_builtin_arity(MinimBuiltin fun, MinimArity *parity)
{
    // Miscellaneous
    SET_ARITY_EXACT(equalp, 2);
    SET_ARITY_EXACT(symbolp, 1);
    SET_ARITY_MIN(printf, 1);
    SET_ARITY_RANGE(error, 1, 2);
    SET_ARITY_EXACT(void, 0);
    SET_ARITY_EXACT(version, 0);
    SET_ARITY_EXACT(symbol_count, 0);

    // Syntax
    SET_ARITY_EXACT(unwrap, 1);
    SET_ARITY_RANGE(syntax_error, 2, 4);

    // Arity
    SET_ARITY_EXACT(procedurep, 1);
    SET_ARITY_EXACT(procedure_arity, 1);
    
    // Boolean
    SET_ARITY_EXACT(boolp, 1);
    SET_ARITY_EXACT(not, 1);

    // Number
    SET_ARITY_EXACT(numberp, 1);
    SET_ARITY_EXACT(zerop, 1);
    SET_ARITY_EXACT(positivep, 1);
    SET_ARITY_EXACT(negativep, 1);
    SET_ARITY_EXACT(evenp, 1);
    SET_ARITY_EXACT(oddp, 1);
    SET_ARITY_EXACT(exactp, 1);
    SET_ARITY_EXACT(inexactp, 1);
    SET_ARITY_EXACT(integerp, 1);
    SET_ARITY_EXACT(nanp, 1);
    SET_ARITY_EXACT(infinitep, 1);
    SET_ARITY_MIN(eq, 1);
    SET_ARITY_MIN(gt, 1);
    SET_ARITY_MIN(lt, 1);
    SET_ARITY_MIN(gte, 1);
    SET_ARITY_MIN(lte, 1);
    SET_ARITY_EXACT(to_exact, 1);
    SET_ARITY_EXACT(to_inexact, 1);

    // String
    SET_ARITY_EXACT(stringp, 1);
    SET_ARITY_MIN(string_append, 1);
    SET_ARITY_RANGE(substring, 2, 3);
    SET_ARITY_EXACT(string_to_symbol, 1);
    SET_ARITY_EXACT(symbol_to_string, 1);
    SET_ARITY_MIN(format, 1);

    // Pair
    SET_ARITY_EXACT(cons, 2);
    SET_ARITY_EXACT(consp, 1);
    SET_ARITY_EXACT(car, 1);
    SET_ARITY_EXACT(cdr, 1);

    SET_ARITY_EXACT(caar, 1);
    SET_ARITY_EXACT(cadr, 1);
    SET_ARITY_EXACT(cdar, 1);
    SET_ARITY_EXACT(cddr, 1);

    // List
    // NO CHECK: list
    SET_ARITY_EXACT(listp, 1);
    SET_ARITY_EXACT(nullp, 1);
    SET_ARITY_EXACT(head, 1);
    SET_ARITY_EXACT(tail, 1);
    SET_ARITY_EXACT(length, 1);
    SET_ARITY_MIN(append, 1);
    SET_ARITY_EXACT(reverse, 1);
    SET_ARITY_EXACT(remove, 2);
    SET_ARITY_EXACT(member, 2);
    SET_ARITY_EXACT(list_ref, 2);
    SET_ARITY_EXACT(map, 2);
    SET_ARITY_MIN(apply, 2);
    SET_ARITY_EXACT(filter, 2);
    SET_ARITY_EXACT(filtern, 2);
    SET_ARITY_EXACT(foldl, 3);
    SET_ARITY_EXACT(foldr, 3);
    SET_ARITY_EXACT(assoc, 2);

    // Hash table
    SET_ARITY_EXACT(hash, 0);
    SET_ARITY_EXACT(hashp, 1);
    SET_ARITY_EXACT(hash_keyp, 2);
    SET_ARITY_EXACT(hash_ref, 2);
    SET_ARITY_EXACT(hash_remove, 2);
    SET_ARITY_EXACT(hash_set, 3);
    SET_ARITY_EXACT(hash_setb, 3);
    SET_ARITY_EXACT(hash_removeb, 2);
    SET_ARITY_EXACT(hash_to_list, 1);

    // Vector
    SET_ARITY_RANGE(make_vector, 1, 2);
    // NO CHECK: 'vector'
    SET_ARITY_EXACT(vector_length, 1);
    SET_ARITY_EXACT(vectorp, 1);
    SET_ARITY_EXACT(vector_ref, 2);
    SET_ARITY_EXACT(vector_setb, 3);
    SET_ARITY_EXACT(vector_to_list, 1);
    SET_ARITY_EXACT(list_to_vector, 1);
    SET_ARITY_EXACT(vector_fillb, 2);

    // Sequence
    SET_ARITY_EXACT(sequencep, 1);
    SET_ARITY_RANGE(in_range, 1, 2);
    SET_ARITY_RANGE(in_naturals, 0, 1);
    SET_ARITY_EXACT(sequence_to_list, 1);

    // Math
    // NO CHECK: 'add'
    SET_ARITY_MIN(sub, 1);
    // NO CHECK: 'mul'
    SET_ARITY_MIN(div, 1);
    SET_ARITY_MIN(max, 1);
    SET_ARITY_MIN(min, 1);
    SET_ARITY_EXACT(abs, 1);
    SET_ARITY_EXACT(modulo, 2);
    SET_ARITY_EXACT(remainder, 2);
    SET_ARITY_EXACT(quotient, 2);
    SET_ARITY_EXACT(numerator, 1);
    SET_ARITY_EXACT(denominator, 1);
    SET_ARITY_EXACT(gcd, 2);
    SET_ARITY_EXACT(lcm, 2);

    SET_ARITY_EXACT(floor, 1);
    SET_ARITY_EXACT(ceil, 1);
    SET_ARITY_EXACT(trunc, 1);
    SET_ARITY_EXACT(round, 1);

    SET_ARITY_EXACT(sqrt, 1);
    SET_ARITY_EXACT(exp, 1);
    SET_ARITY_EXACT(log, 1);
    SET_ARITY_EXACT(pow, 2);

    SET_ARITY_EXACT(sin, 1);
    SET_ARITY_EXACT(cos, 1);
    SET_ARITY_EXACT(tan, 1);
    SET_ARITY_EXACT(asin, 1);
    SET_ARITY_EXACT(acos, 1);
    SET_ARITY_EXACT(atan, 1);

    // Promise
    SET_ARITY_EXACT(promisep, 1);
    
    parity->low = 0;
    parity->high = SIZE_MAX;
    return true;
}    

bool minim_get_syntax_arity(MinimBuiltin fun, MinimArity *parity)
{
    SET_ARITY_MIN(def, 2);
    SET_ARITY_EXACT(setb, 2);
    SET_ARITY_EXACT(if, 3);
    // no check: 'case'
    SET_ARITY_RANGE(let, 2, 3);
    SET_ARITY_RANGE(letstar, 2, 3);
    SET_ARITY_EXACT(for, 2);
    SET_ARITY_EXACT(for_list, 2);
    SET_ARITY_MIN(begin, 1);
    SET_ARITY_EXACT(quote, 1);
    SET_ARITY_EXACT(quasiquote, 1);
    SET_ARITY_EXACT(unquote, 1);
    SET_ARITY_MIN(lambda, 2);
    SET_ARITY_EXACT(exit, 0);  // for now
    SET_ARITY_EXACT(delay, 1);
    SET_ARITY_EXACT(force, 1);

    SET_ARITY_EXACT(def_syntax, 2);
    SET_ARITY_EXACT(syntax, 1);
    SET_ARITY_EXACT(syntaxp, 1);

    parity->low = 0;
    parity->high = SIZE_MAX;
    return true;
}

bool minim_check_arity(MinimBuiltin fun, size_t argc, MinimEnv *env, MinimObject **perr)
{
    MinimArity arity;

    if (!minim_get_builtin_arity(fun, &arity))
        return false;

    if (IS_OUTSIDE(argc, arity.low, arity.high))
    {
        builtin_arity_error(fun, argc, arity.low, arity.high, env, perr);
        return false;
    }

    return true;
}

bool minim_check_syntax_arity(MinimBuiltin fun, size_t argc, MinimEnv *env)
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
    MinimObject *res;
    
    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_FUNCP(args[0]));
    return res;
}

MinimObject *minim_builtin_procedure_arity(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *min, *max;
    MinimArity arity;
    
    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("procedure", "procedure-arity", 0, args[0]);

    if (MINIM_OBJ_BUILTINP(args[0]))
    {
        minim_get_builtin_arity(MINIM_DATA(args[0]), &arity);
    }
    else
    {
        MinimLambda *lam = MINIM_DATA(args[0]);

        arity.low = lam->argc;
        arity.high = (lam->rest) ? SIZE_MAX : arity.low;
    }

    if (arity.low == arity.high)
    {
        res = uint_to_minim_number(arity.low);
    }
    else
    {
        min = uint_to_minim_number(arity.low);
        if (arity.high == SIZE_MAX)
            init_minim_object(&max, MINIM_OBJ_BOOL, 0);
        else
            max = uint_to_minim_number(arity.high);
        init_minim_object(&res, MINIM_OBJ_PAIR, min, max);
    }

    return res;
}

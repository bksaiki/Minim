#include <math.h>
#include <stdarg.h>
#include "builtin.h"
#include "error.h"
#include "number.h"

void minim_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    initv_minim_object(&obj, type, rest);
    env_intern_sym(env, name, obj);
    va_end(rest);
}

void minim_load_builtins(MinimEnv *env)
{
    // Variable / Control
    minim_load_builtin(env, "def", MINIM_OBJ_SYNTAX, minim_builtin_def);
    minim_load_builtin(env, "set!", MINIM_OBJ_SYNTAX, minim_builtin_setb);
    minim_load_builtin(env, "if", MINIM_OBJ_SYNTAX, minim_builtin_if);
    minim_load_builtin(env, "unless", MINIM_OBJ_SYNTAX, minim_builtin_unless);
    minim_load_builtin(env, "when", MINIM_OBJ_SYNTAX, minim_builtin_when);
    minim_load_builtin(env, "cond", MINIM_OBJ_SYNTAX, minim_builtin_cond);
    minim_load_builtin(env, "let", MINIM_OBJ_SYNTAX, minim_builtin_let);
    minim_load_builtin(env, "let*", MINIM_OBJ_SYNTAX, minim_builtin_letstar);
    minim_load_builtin(env, "for", MINIM_OBJ_SYNTAX, minim_builtin_for);
    minim_load_builtin(env, "for-list", MINIM_OBJ_SYNTAX, minim_builtin_for_list);
    minim_load_builtin(env, "begin", MINIM_OBJ_SYNTAX, minim_builtin_begin);
    minim_load_builtin(env, "quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);
    minim_load_builtin(env, "lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
    minim_load_builtin(env, "exit", MINIM_OBJ_SYNTAX, minim_builtin_exit);
    
    // Miscellaneous
    minim_load_builtin(env, "equal?", MINIM_OBJ_FUNC, minim_builtin_equalp);
    minim_load_builtin(env, "symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
    minim_load_builtin(env, "printf", MINIM_OBJ_FUNC, minim_builtin_printf);
    minim_load_builtin(env, "error", MINIM_OBJ_FUNC, minim_builtin_error);
    minim_load_builtin(env, "version", MINIM_OBJ_FUNC, minim_builtin_version);
    minim_load_builtin(env, "symbol-count", MINIM_OBJ_FUNC, minim_builtin_symbol_count);

    // Boolean
    minim_load_builtin(env, "true", MINIM_OBJ_BOOL, 1);
    minim_load_builtin(env, "false", MINIM_OBJ_BOOL, 0);
    minim_load_builtin(env, "bool?", MINIM_OBJ_FUNC, minim_builtin_boolp);
    minim_load_builtin(env, "not", MINIM_OBJ_FUNC, minim_builtin_not);
    minim_load_builtin(env, "or", MINIM_OBJ_FUNC, minim_builtin_or);
    minim_load_builtin(env, "and", MINIM_OBJ_FUNC, minim_builtin_and);

    // Numbers
    minim_load_builtin(env, "number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    minim_load_builtin(env, "zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
    minim_load_builtin(env, "positive?", MINIM_OBJ_FUNC, minim_builtin_positivep);
    minim_load_builtin(env, "negative?", MINIM_OBJ_FUNC, minim_builtin_negativep);
    minim_load_builtin(env, "even?", MINIM_OBJ_FUNC, minim_builtin_evenp);
    minim_load_builtin(env, "odd?", MINIM_OBJ_FUNC, minim_builtin_oddp);
    minim_load_builtin(env, "exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    minim_load_builtin(env, "inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);
    minim_load_builtin(env, "integer?", MINIM_OBJ_FUNC, minim_builtin_integerp);
    minim_load_builtin(env, "exact-integer?", MINIM_OBJ_FUNC, minim_builtin_exact_integerp);
    minim_load_builtin(env, "nan?", MINIM_OBJ_FUNC, minim_builtin_nanp);
    minim_load_builtin(env, "infinite?", MINIM_OBJ_FUNC, minim_builtin_infinitep);
    
    minim_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    minim_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    minim_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    minim_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    minim_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);

    minim_load_builtin(env, "exact", MINIM_OBJ_FUNC, minim_builtin_to_exact);
    minim_load_builtin(env, "inexact", MINIM_OBJ_FUNC, minim_builtin_to_inexact);

    minim_load_builtin(env, "pi", MINIM_OBJ_INEXACT, 3.141592653589793238465);
    minim_load_builtin(env, "phi", MINIM_OBJ_INEXACT, 1.618033988749894848204);
    minim_load_builtin(env, "inf", MINIM_OBJ_INEXACT, INFINITY);
    minim_load_builtin(env, "-inf", MINIM_OBJ_INEXACT, -INFINITY);
    minim_load_builtin(env, "nan", MINIM_OBJ_INEXACT, NAN);

    // String
    minim_load_builtin(env, "string?", MINIM_OBJ_FUNC, minim_builtin_stringp);
    minim_load_builtin(env, "string-append", MINIM_OBJ_FUNC, minim_builtin_string_append);
    minim_load_builtin(env, "substring", MINIM_OBJ_FUNC, minim_builtin_substring);
    minim_load_builtin(env, "string->symbol", MINIM_OBJ_FUNC, minim_builtin_string_to_symbol);
    minim_load_builtin(env, "symbol->string", MINIM_OBJ_FUNC, minim_builtin_symbol_to_string);
    minim_load_builtin(env, "format", MINIM_OBJ_FUNC, minim_builtin_format);
    
    // Pair
    minim_load_builtin(env, "cons", MINIM_OBJ_FUNC, minim_builtin_cons);
    minim_load_builtin(env, "pair?", MINIM_OBJ_FUNC, minim_builtin_consp);
    minim_load_builtin(env, "car", MINIM_OBJ_FUNC, minim_builtin_car);
    minim_load_builtin(env, "cdr", MINIM_OBJ_FUNC, minim_builtin_cdr);
    minim_load_builtin(env, "null", MINIM_OBJ_PAIR, NULL, NULL);

    minim_load_builtin(env, "caar", MINIM_OBJ_FUNC, minim_builtin_caar);
    minim_load_builtin(env, "cadr", MINIM_OBJ_FUNC, minim_builtin_cadr);
    minim_load_builtin(env, "cdar", MINIM_OBJ_FUNC, minim_builtin_cdar);
    minim_load_builtin(env, "cddr", MINIM_OBJ_FUNC, minim_builtin_cddr);

    // List
    minim_load_builtin(env, "list", MINIM_OBJ_FUNC, minim_builtin_list);
    minim_load_builtin(env, "list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    minim_load_builtin(env, "null?", MINIM_OBJ_FUNC, minim_builtin_nullp);
    minim_load_builtin(env, "head", MINIM_OBJ_FUNC, minim_builtin_head);
    minim_load_builtin(env, "tail", MINIM_OBJ_FUNC, minim_builtin_tail);
    minim_load_builtin(env, "length", MINIM_OBJ_FUNC, minim_builtin_length);
    minim_load_builtin(env, "append", MINIM_OBJ_FUNC, minim_builtin_append);
    minim_load_builtin(env, "reverse", MINIM_OBJ_FUNC, minim_builtin_reverse);
    minim_load_builtin(env, "remove", MINIM_OBJ_FUNC, minim_builtin_remove);
    minim_load_builtin(env, "member", MINIM_OBJ_FUNC, minim_builtin_member);
    minim_load_builtin(env, "list-ref", MINIM_OBJ_FUNC, minim_builtin_list_ref);
    minim_load_builtin(env, "map", MINIM_OBJ_FUNC, minim_builtin_map);
    minim_load_builtin(env, "apply", MINIM_OBJ_FUNC, minim_builtin_apply);
    minim_load_builtin(env, "filter", MINIM_OBJ_FUNC, minim_builtin_filter);
    minim_load_builtin(env, "filtern", MINIM_OBJ_FUNC, minim_builtin_filtern);
    minim_load_builtin(env, "foldl", MINIM_OBJ_FUNC, minim_builtin_foldl);
    minim_load_builtin(env, "foldr", MINIM_OBJ_FUNC, minim_builtin_foldr);
    minim_load_builtin(env, "assoc", MINIM_OBJ_FUNC, minim_builtin_assoc);

    // Hash table
    minim_load_builtin(env, "hash", MINIM_OBJ_FUNC, minim_builtin_hash);
    minim_load_builtin(env, "hash?", MINIM_OBJ_FUNC, minim_builtin_hashp);
    minim_load_builtin(env, "hash-key?", MINIM_OBJ_FUNC, minim_builtin_hash_keyp);
    minim_load_builtin(env, "hash-ref", MINIM_OBJ_FUNC, minim_builtin_hash_ref);
    minim_load_builtin(env, "hash-remove", MINIM_OBJ_FUNC, minim_builtin_hash_remove);
    minim_load_builtin(env, "hash-set", MINIM_OBJ_FUNC, minim_builtin_hash_set);
    minim_load_builtin(env, "hash-set!", MINIM_OBJ_FUNC, minim_builtin_hash_setb);
    minim_load_builtin(env, "hash-remove!", MINIM_OBJ_FUNC, minim_builtin_hash_removeb);
    minim_load_builtin(env, "hash->list", MINIM_OBJ_FUNC, minim_builtin_hash_to_list);

    // Vector
    minim_load_builtin(env, "vector", MINIM_OBJ_FUNC, minim_builtin_vector);
    minim_load_builtin(env, "make-vector", MINIM_OBJ_FUNC, minim_builtin_make_vector);
    minim_load_builtin(env, "vector-length", MINIM_OBJ_FUNC, minim_builtin_vector_length);
    minim_load_builtin(env, "vector-ref", MINIM_OBJ_FUNC, minim_builtin_vector_ref);
    minim_load_builtin(env, "vector-set!", MINIM_OBJ_FUNC, minim_builtin_vector_setb);
    minim_load_builtin(env, "vector->list", MINIM_OBJ_FUNC, minim_builtin_vector_to_list);
    minim_load_builtin(env, "list->vector", MINIM_OBJ_FUNC, minim_builtin_list_to_vector);
    minim_load_builtin(env, "vector-fill!", MINIM_OBJ_FUNC, minim_builtin_vector_fillb);

    // Sequence
    minim_load_builtin(env, "sequence?", MINIM_OBJ_FUNC, minim_builtin_sequencep);
    minim_load_builtin(env, "in-range", MINIM_OBJ_FUNC, minim_builtin_in_range);
    minim_load_builtin(env, "in-naturals", MINIM_OBJ_FUNC, minim_builtin_in_naturals);
    minim_load_builtin(env, "sequence->list", MINIM_OBJ_FUNC, minim_builtin_sequence_to_list);

    // Math
    minim_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    minim_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_sub);
    minim_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_mul);
    minim_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);
    minim_load_builtin(env, "abs", MINIM_OBJ_FUNC, minim_builtin_abs);
    minim_load_builtin(env, "max", MINIM_OBJ_FUNC, minim_builtin_max);
    minim_load_builtin(env, "min", MINIM_OBJ_FUNC, minim_builtin_min);
    minim_load_builtin(env, "modulo", MINIM_OBJ_FUNC, minim_builtin_modulo);
    minim_load_builtin(env, "remainder", MINIM_OBJ_FUNC, minim_builtin_remainder);
    minim_load_builtin(env, "quotient", MINIM_OBJ_FUNC, minim_builtin_quotient);
    minim_load_builtin(env, "numerator", MINIM_OBJ_FUNC, minim_builtin_numerator);
    minim_load_builtin(env, "denominator", MINIM_OBJ_FUNC, minim_builtin_denominator);
    minim_load_builtin(env, "gcd", MINIM_OBJ_FUNC, minim_builtin_gcd);
    minim_load_builtin(env, "lcm", MINIM_OBJ_FUNC, minim_builtin_lcm);

    minim_load_builtin(env, "floor", MINIM_OBJ_FUNC, minim_builtin_floor);
    minim_load_builtin(env, "ceil", MINIM_OBJ_FUNC, minim_builtin_ceil);
    minim_load_builtin(env, "trunc", MINIM_OBJ_FUNC, minim_builtin_trunc);
    minim_load_builtin(env, "round", MINIM_OBJ_FUNC, minim_builtin_round);

    minim_load_builtin(env, "sqrt", MINIM_OBJ_FUNC, minim_builtin_sqrt);
    minim_load_builtin(env, "exp", MINIM_OBJ_FUNC, minim_builtin_exp);
    minim_load_builtin(env, "log", MINIM_OBJ_FUNC, minim_builtin_log);
    minim_load_builtin(env, "pow", MINIM_OBJ_FUNC, minim_builtin_pow);

    minim_load_builtin(env, "sin", MINIM_OBJ_FUNC, minim_builtin_sin);
    minim_load_builtin(env, "cos", MINIM_OBJ_FUNC, minim_builtin_cos);
    minim_load_builtin(env, "tan", MINIM_OBJ_FUNC, minim_builtin_tan);
    minim_load_builtin(env, "asin", MINIM_OBJ_FUNC, minim_builtin_asin);
    minim_load_builtin(env, "acos", MINIM_OBJ_FUNC, minim_builtin_acos);
    minim_load_builtin(env, "atan", MINIM_OBJ_FUNC, minim_builtin_atan);
}

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
    free_minim_object(obj);
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
    SET_ARITY_MIN(def, 2);
    SET_ARITY_EXACT(setb, 2);
    SET_ARITY_EXACT(if, 3);
    SET_ARITY_MIN(unless, 2);
    SET_ARITY_MIN(when, 2);
    // NO CHECK: 'cond'
    SET_ARITY_EXACT(let, 2);
    SET_ARITY_EXACT(letstar, 2);
    SET_ARITY_EXACT(for, 2);
    SET_ARITY_EXACT(for_list, 2);
    SET_ARITY_MIN(begin, 1);
    SET_ARITY_EXACT(quote, 1);
    SET_ARITY_MIN(lambda, 2);
    SET_ARITY_EXACT(exit, 0);  // for now

    // Miscellaneous
    SET_ARITY_EXACT(equalp, 2);
    SET_ARITY_EXACT(symbolp, 1);
    SET_ARITY_MIN(printf, 1);
    SET_ARITY_RANGE(error, 1, 2);
    SET_ARITY_EXACT(version, 0);
    SET_ARITY_EXACT(symbol_count, 0);
    
    // Boolean
    SET_ARITY_EXACT(boolp, 1);
    SET_ARITY_EXACT(not, 1);
    SET_ARITY_MIN(or, 1);
    SET_ARITY_MIN(and, 1);

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
    SET_ARITY_EXACT(exact_integerp, 1);
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

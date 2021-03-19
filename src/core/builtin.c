#include <stdarg.h>
#include "builtin.h"
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
    minim_load_builtin(env, "exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    minim_load_builtin(env, "inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);
    minim_load_builtin(env, "integer?", MINIM_OBJ_FUNC, minim_builtin_integerp);
    minim_load_builtin(env, "exact-integer?", MINIM_OBJ_FUNC, minim_builtin_exact_integerp);
    
    minim_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    minim_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    minim_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    minim_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    minim_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);

    minim_load_builtin(env, "exact", MINIM_OBJ_FUNC, minim_builtin_to_exact);
    minim_load_builtin(env, "inexact", MINIM_OBJ_FUNC, minim_builtin_to_inexact);

    minim_load_builtin(env, "pi", MINIM_OBJ_NUM, minim_number_pi());
    minim_load_builtin(env, "phi", MINIM_OBJ_NUM, minim_number_phi());
    minim_load_builtin(env, "inf", MINIM_OBJ_NUM, minim_number_inf());
    minim_load_builtin(env, "-inf", MINIM_OBJ_NUM, minim_number_ninf());
    minim_load_builtin(env, "nan", MINIM_OBJ_NUM, minim_number_nan());

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

    // List
    minim_load_builtin(env, "list", MINIM_OBJ_FUNC, minim_builtin_list);
    minim_load_builtin(env, "list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    minim_load_builtin(env, "null?", MINIM_OBJ_FUNC, minim_builtin_nullp);
    minim_load_builtin(env, "head", MINIM_OBJ_FUNC, minim_builtin_head);
    minim_load_builtin(env, "tail", MINIM_OBJ_FUNC, minim_builtin_tail);
    minim_load_builtin(env, "length", MINIM_OBJ_FUNC, minim_builtin_length);
    minim_load_builtin(env, "append", MINIM_OBJ_FUNC, minim_builtin_append);
    minim_load_builtin(env, "reverse", MINIM_OBJ_FUNC, minim_builtin_reverse);
    minim_load_builtin(env, "list-ref", MINIM_OBJ_FUNC, minim_builtin_list_ref);
    minim_load_builtin(env, "map", MINIM_OBJ_FUNC, minim_builtin_map);
    minim_load_builtin(env, "apply", MINIM_OBJ_FUNC, minim_builtin_apply);
    minim_load_builtin(env, "filter", MINIM_OBJ_FUNC, minim_builtin_filter);
    minim_load_builtin(env, "filtern", MINIM_OBJ_FUNC, minim_builtin_filtern);
    minim_load_builtin(env, "foldl", MINIM_OBJ_FUNC, minim_builtin_foldl);
    minim_load_builtin(env, "foldr", MINIM_OBJ_FUNC, minim_builtin_foldr);

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
    minim_load_builtin(env, "vector-ref", MINIM_OBJ_FUNC, minim_builtin_vector_ref);
    minim_load_builtin(env, "vector-set!", MINIM_OBJ_FUNC, minim_builtin_vector_setb);
    minim_load_builtin(env, "vector->list", MINIM_OBJ_FUNC, minim_builtin_vector_to_list);
    minim_load_builtin(env, "list->vector", MINIM_OBJ_FUNC, minim_builtin_list_to_vector);

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
    minim_load_builtin(env, "mod", MINIM_OBJ_FUNC, minim_builtin_modulo);
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

    if (min == max)              // exact
    {
        if (min == 1)   minim_error(perr, "Expected 1 argument for '%s'", name);
        else            minim_error(perr, "Expected %lu arguments for '%s'", min, name);
    }
    else if (max == SIZE_MAX)   // min
    {
        if (min == 1)   minim_error(perr, "Expected at least 1 argument for '%s'", name);
        else            minim_error(perr, "Expected at least %lu arguments for '%s'", min, name);
    }
    else                        // range
    {
        minim_error(perr, "Expected between %lu and %lu arguments for '%s'", min, max, name);
    }
}

#define CHECK_RANGE_ARITY(fun, argc, env, perr, builtin, min, max)      \
{                                                                       \
    if (BUILTIN_EQUALP(fun, builtin))                                   \
    {                                                                   \
        if (IS_OUTSIDE(argc, min, max))                                 \
        {                                                               \
            builtin_arity_error(FULL_BUILTIN_NAME(builtin), argc,       \
                                min, max, env, perr);                   \
            return false;                                               \
        }                                                               \
    }                                                                   \
}

#define CHECK_EXACT_ARITY(fun, argc, env, perr, builtin, expected)  \
    CHECK_RANGE_ARITY(fun, argc, env, perr, builtin, expected, expected)  

#define CHECK_MIN_ARITY(fun, argc, env, perr, builtin, min)         \
    CHECK_RANGE_ARITY(fun, argc, env, perr, builtin, min, SIZE_MAX)       

bool minim_check_arity(MinimBuiltin fun, size_t argc, MinimEnv *env, MinimObject **perr)
{
    // Variable / Control
    CHECK_RANGE_ARITY(fun, argc, env, perr, def, 2, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, setb, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, if, 3);
    CHECK_MIN_ARITY(fun, argc, env, perr, unless, 2);
    CHECK_MIN_ARITY(fun, argc, env, perr, when, 2);
    // NO CHECK: 'cond'
    CHECK_EXACT_ARITY(fun, argc, env, perr, let, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, letstar, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, for, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, for_list, 2);
    CHECK_MIN_ARITY(fun, argc, env, perr, begin, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, quote, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, lambda, 2);

    // Miscellaneous
    CHECK_EXACT_ARITY(fun, argc, env, perr, equalp, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, symbolp, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, printf, 1);
    CHECK_RANGE_ARITY(fun, argc, env, perr, error, 1, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, version, 0);
    CHECK_EXACT_ARITY(fun, argc, env, perr, symbol_count, 0);
    
    // Boolean
    CHECK_EXACT_ARITY(fun, argc, env, perr, boolp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, not, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, or, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, and, 1);

    // Number
    CHECK_EXACT_ARITY(fun, argc, env, perr, numberp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, zerop, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, positivep, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, negativep, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, exactp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, inexactp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, integerp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, exact_integerp, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, eq, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, gt, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, lt, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, gte, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, lte, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, to_exact, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, to_inexact, 1);

    // String
    CHECK_EXACT_ARITY(fun, argc, env, perr, stringp, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, string_append, 1);
    CHECK_RANGE_ARITY(fun, argc, env, perr, substring, 2, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, string_to_symbol, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, symbol_to_string, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, format, 1);

    // Pair
    CHECK_EXACT_ARITY(fun, argc, env, perr, cons, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, consp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, car, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, cdr, 1);

    // List
    // NO CHECK: list
    CHECK_EXACT_ARITY(fun, argc, env, perr, listp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, nullp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, head, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, tail, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, length, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, append, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, reverse, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, list_ref, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, map, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, apply, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, filter, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, filtern, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, foldl, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, foldr, 3);

    // Hash table
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash, 0);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hashp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_keyp, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_ref, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_remove, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_set, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_setb, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_removeb, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, hash_to_list, 1);

    // Vector
    CHECK_EXACT_ARITY(fun, argc, env, perr, make_vector, 1);
    // NO CHECK: 'vector'
    CHECK_EXACT_ARITY(fun, argc, env, perr, vector_ref, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, vector_setb, 3);
    CHECK_EXACT_ARITY(fun, argc, env, perr, vector_to_list, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, list_to_vector, 1);

    // Sequence
    CHECK_EXACT_ARITY(fun, argc, env, perr, sequencep, 1);
    CHECK_RANGE_ARITY(fun, argc, env, perr, in_range, 1, 2);
    CHECK_RANGE_ARITY(fun, argc, env, perr, in_naturals, 0, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, sequence_to_list, 1);

    // Math
    CHECK_MIN_ARITY(fun, argc, env, perr, add, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, sub, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, mul, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, div, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, max, 1);
    CHECK_MIN_ARITY(fun, argc, env, perr, min, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, abs, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, sqrt, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, modulo, 2);

    CHECK_EXACT_ARITY(fun, argc, env, perr, exp, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, log, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, pow, 2);
    CHECK_EXACT_ARITY(fun, argc, env, perr, sin, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, cos, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, tan, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, asin, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, acos, 1);
    CHECK_EXACT_ARITY(fun, argc, env, perr, atan, 1);

    return true;
}
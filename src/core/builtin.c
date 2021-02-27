#include <stdarg.h>
#include "builtin.h"

void minim_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    if (type == MINIM_OBJ_FUNC || type == MINIM_OBJ_SYNTAX)
    {
        init_minim_object(&obj, type, va_arg(rest, MinimBuiltin));
    }
    else if (type == MINIM_OBJ_BOOL)
    {
        init_minim_object(&obj, type, va_arg(rest, int));
    }

    va_end(rest);
    env_intern_sym(env, name, obj);
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
    minim_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    minim_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    minim_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    minim_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    minim_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);

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
    minim_load_builtin(env, "sqrt", MINIM_OBJ_FUNC, minim_builtin_sqrt);
    minim_load_builtin(env, "exp", MINIM_OBJ_FUNC, minim_builtin_exp);
    minim_load_builtin(env, "log", MINIM_OBJ_FUNC, minim_builtin_log);
    minim_load_builtin(env, "pow", MINIM_OBJ_FUNC, minim_builtin_pow);
}
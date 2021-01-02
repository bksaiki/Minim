#include <stdarg.h>
#include "builtin.h"

void env_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
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

void env_load_builtins(MinimEnv *env)
{
    // Variable / Control
    env_load_builtin(env, "def", MINIM_OBJ_SYNTAX, minim_builtin_def);
    env_load_builtin(env, "set!", MINIM_OBJ_SYNTAX, minim_builtin_setb);
    env_load_builtin(env, "if", MINIM_OBJ_SYNTAX, minim_builtin_if);
    env_load_builtin(env, "let", MINIM_OBJ_SYNTAX, minim_builtin_let);
    env_load_builtin(env, "let*", MINIM_OBJ_SYNTAX, minim_builtin_letstar);
    env_load_builtin(env, "for", MINIM_OBJ_SYNTAX, minim_builtin_for);
    env_load_builtin(env, "for-list", MINIM_OBJ_SYNTAX, minim_builtin_for_list);
    env_load_builtin(env, "begin", MINIM_OBJ_SYNTAX, minim_builtin_begin);
    env_load_builtin(env, "quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);
    env_load_builtin(env, "lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
    
    // Miscellaneous
    env_load_builtin(env, "equal?", MINIM_OBJ_FUNC, minim_builtin_equalp);
    env_load_builtin(env, "symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
    env_load_builtin(env, "printf", MINIM_OBJ_FUNC, minim_builtin_printf);

    // Boolean
    env_load_builtin(env, "true", MINIM_OBJ_BOOL, 1);
    env_load_builtin(env, "false", MINIM_OBJ_BOOL, 0);
    env_load_builtin(env, "bool?", MINIM_OBJ_FUNC, minim_builtin_boolp);
    env_load_builtin(env, "not", MINIM_OBJ_FUNC, minim_builtin_not);
    env_load_builtin(env, "or", MINIM_OBJ_FUNC, minim_builtin_or);
    env_load_builtin(env, "and", MINIM_OBJ_FUNC, minim_builtin_and);

    // Numbers
    env_load_builtin(env, "number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    env_load_builtin(env, "zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
    env_load_builtin(env, "positive?", MINIM_OBJ_FUNC, minim_builtin_positivep);
    env_load_builtin(env, "negative?", MINIM_OBJ_FUNC, minim_builtin_negativep);
    env_load_builtin(env, "exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    env_load_builtin(env, "inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);
    env_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    env_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    env_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    env_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    env_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);

    // String
    env_load_builtin(env, "string?", MINIM_OBJ_FUNC, minim_builtin_stringp);
    env_load_builtin(env, "string-append", MINIM_OBJ_FUNC, minim_builtin_string_append);
    env_load_builtin(env, "substring", MINIM_OBJ_FUNC, minim_builtin_substring);
    env_load_builtin(env, "string->symbol", MINIM_OBJ_FUNC, minim_builtin_string_to_symbol);
    env_load_builtin(env, "symbol->string", MINIM_OBJ_FUNC, minim_builtin_symbol_to_string);
    env_load_builtin(env, "format", MINIM_OBJ_FUNC, minim_builtin_format);
    
    // Pair
    env_load_builtin(env, "cons", MINIM_OBJ_FUNC, minim_builtin_cons);
    env_load_builtin(env, "pair?", MINIM_OBJ_FUNC, minim_builtin_consp);
    env_load_builtin(env, "car", MINIM_OBJ_FUNC, minim_builtin_car);
    env_load_builtin(env, "cdr", MINIM_OBJ_FUNC, minim_builtin_cdr);

    // List
    env_load_builtin(env, "list", MINIM_OBJ_FUNC, minim_builtin_list);
    env_load_builtin(env, "list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    env_load_builtin(env, "null?", MINIM_OBJ_FUNC, minim_builtin_nullp);
    env_load_builtin(env, "head", MINIM_OBJ_FUNC, minim_builtin_head);
    env_load_builtin(env, "tail", MINIM_OBJ_FUNC, minim_builtin_tail);
    env_load_builtin(env, "length", MINIM_OBJ_FUNC, minim_builtin_length);
    env_load_builtin(env, "append", MINIM_OBJ_FUNC, minim_builtin_append);
    env_load_builtin(env, "reverse", MINIM_OBJ_FUNC, minim_builtin_reverse);
    env_load_builtin(env, "list-ref", MINIM_OBJ_FUNC, minim_builtin_list_ref);
    env_load_builtin(env, "map", MINIM_OBJ_FUNC, minim_builtin_map);
    env_load_builtin(env, "apply", MINIM_OBJ_FUNC, minim_builtin_apply);
    env_load_builtin(env, "filter", MINIM_OBJ_FUNC, minim_builtin_filter);
    env_load_builtin(env, "filtern", MINIM_OBJ_FUNC, minim_builtin_filtern);

    // Hash table
    env_load_builtin(env, "hash", MINIM_OBJ_FUNC, minim_builtin_hash);
    env_load_builtin(env, "hash?", MINIM_OBJ_FUNC, minim_builtin_hashp);
    env_load_builtin(env, "hash-key?", MINIM_OBJ_FUNC, minim_builtin_hash_keyp);
    env_load_builtin(env, "hash-ref", MINIM_OBJ_FUNC, minim_builtin_hash_ref);
    env_load_builtin(env, "hash-remove", MINIM_OBJ_FUNC, minim_builtin_hash_remove);
    env_load_builtin(env, "hash-set", MINIM_OBJ_FUNC, minim_builtin_hash_set);
    env_load_builtin(env, "hash-set!", MINIM_OBJ_FUNC, minim_builtin_hash_setb);
    env_load_builtin(env, "hash-remove!", MINIM_OBJ_FUNC, minim_builtin_hash_removeb);
    env_load_builtin(env, "hash->list", MINIM_OBJ_FUNC, minim_builtin_hash_to_list);

    // Vector
    env_load_builtin(env, "vector", MINIM_OBJ_FUNC, minim_builtin_vector);
    env_load_builtin(env, "make-vector", MINIM_OBJ_FUNC, minim_builtin_make_vector);
    env_load_builtin(env, "vector-ref", MINIM_OBJ_FUNC, minim_builtin_vector_ref);
    env_load_builtin(env, "vector-set!", MINIM_OBJ_FUNC, minim_builtin_vector_setb);
    env_load_builtin(env, "vector->list", MINIM_OBJ_FUNC, minim_builtin_vector_to_list);
    env_load_builtin(env, "list->vector", MINIM_OBJ_FUNC, minim_builtin_list_to_vector);

    // Sequence
    env_load_builtin(env, "sequence?", MINIM_OBJ_FUNC, minim_builtin_sequencep);
    env_load_builtin(env, "in-range", MINIM_OBJ_FUNC, minim_builtin_in_range);

    // Math
    env_load_builtin(env, "+", MINIM_OBJ_FUNC, minim_builtin_add);
    env_load_builtin(env, "-", MINIM_OBJ_FUNC, minim_builtin_sub);
    env_load_builtin(env, "*", MINIM_OBJ_FUNC, minim_builtin_mul);
    env_load_builtin(env, "/", MINIM_OBJ_FUNC, minim_builtin_div);
    env_load_builtin(env, "sqrt", MINIM_OBJ_FUNC, minim_builtin_sqrt);
    env_load_builtin(env, "exp", MINIM_OBJ_FUNC, minim_builtin_exp);
    env_load_builtin(env, "log", MINIM_OBJ_FUNC, minim_builtin_log);
    env_load_builtin(env, "pow", MINIM_OBJ_FUNC, minim_builtin_pow);
}
#include <math.h>
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
    minim_load_builtin(env, "let", MINIM_OBJ_SYNTAX, minim_builtin_let);
    minim_load_builtin(env, "let*", MINIM_OBJ_SYNTAX, minim_builtin_letstar);
    minim_load_builtin(env, "begin", MINIM_OBJ_SYNTAX, minim_builtin_begin);
    minim_load_builtin(env, "case", MINIM_OBJ_SYNTAX, minim_builtin_case);
    minim_load_builtin(env, "quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);
    minim_load_builtin(env, "quasiquote", MINIM_OBJ_SYNTAX, minim_builtin_quasiquote);
    minim_load_builtin(env, "unquote", MINIM_OBJ_SYNTAX, minim_builtin_unquote);
    minim_load_builtin(env, "lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
    minim_load_builtin(env, "exit", MINIM_OBJ_SYNTAX, minim_builtin_exit);

    // Modules
    minim_load_builtin(env, "%export", MINIM_OBJ_SYNTAX, minim_builtin_export);
    minim_load_builtin(env, "%import", MINIM_OBJ_SYNTAX, minim_builtin_import);

    // Transforms
    minim_load_builtin(env, "def-syntax", MINIM_OBJ_SYNTAX, minim_builtin_def_syntax);
    minim_load_builtin(env, "syntax", MINIM_OBJ_SYNTAX, minim_builtin_syntax);
    minim_load_builtin(env, "syntax?", MINIM_OBJ_FUNC, minim_builtin_syntaxp);
    minim_load_builtin(env, "unwrap", MINIM_OBJ_FUNC, minim_builtin_unwrap);
    minim_load_builtin(env, "syntax-case", MINIM_OBJ_SYNTAX, minim_builtin_syntax_case);
    minim_load_builtin(env, "syntax-error", MINIM_OBJ_FUNC, minim_builtin_syntax_error);
    minim_load_builtin(env, "datum->syntax", MINIM_OBJ_FUNC, minim_builtin_to_syntax);
    
    // Miscellaneous
    minim_load_builtin(env, "equal?", MINIM_OBJ_FUNC, minim_builtin_equalp);
    minim_load_builtin(env, "symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
    minim_load_builtin(env, "printf", MINIM_OBJ_FUNC, minim_builtin_printf);
    minim_load_builtin(env, "error", MINIM_OBJ_FUNC, minim_builtin_error);
    minim_load_builtin(env, "void", MINIM_OBJ_FUNC, minim_builtin_void);
    minim_load_builtin(env, "version", MINIM_OBJ_FUNC, minim_builtin_version);
    minim_load_builtin(env, "symbol-count", MINIM_OBJ_FUNC, minim_builtin_symbol_count);

    // Arity
    minim_load_builtin(env, "procedure?", MINIM_OBJ_FUNC, minim_builtin_procedurep);
    minim_load_builtin(env, "procedure-arity", MINIM_OBJ_FUNC, minim_builtin_procedure_arity);

    // Boolean
    minim_load_builtin(env, "true", MINIM_OBJ_BOOL, 1);
    minim_load_builtin(env, "false", MINIM_OBJ_BOOL, 0);
    minim_load_builtin(env, "bool?", MINIM_OBJ_FUNC, minim_builtin_boolp);
    minim_load_builtin(env, "not", MINIM_OBJ_FUNC, minim_builtin_not);

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
    minim_load_builtin(env, "nan?", MINIM_OBJ_FUNC, minim_builtin_nanp);
    minim_load_builtin(env, "infinite?", MINIM_OBJ_FUNC, minim_builtin_infinitep);
    
    minim_load_builtin(env, "=", MINIM_OBJ_FUNC, minim_builtin_eq);
    minim_load_builtin(env, ">", MINIM_OBJ_FUNC, minim_builtin_gt);
    minim_load_builtin(env, "<", MINIM_OBJ_FUNC, minim_builtin_lt);
    minim_load_builtin(env, ">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    minim_load_builtin(env, "<=", MINIM_OBJ_FUNC, minim_builtin_lte);

    minim_load_builtin(env, "exact", MINIM_OBJ_FUNC, minim_builtin_to_exact);
    minim_load_builtin(env, "inexact", MINIM_OBJ_FUNC, minim_builtin_to_inexact);
    
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
    minim_load_builtin(env, "vector?", MINIM_OBJ_FUNC, minim_builtin_vectorp);
    minim_load_builtin(env, "vector-length", MINIM_OBJ_FUNC, minim_builtin_vector_length);
    minim_load_builtin(env, "vector-ref", MINIM_OBJ_FUNC, minim_builtin_vector_ref);
    minim_load_builtin(env, "vector-set!", MINIM_OBJ_FUNC, minim_builtin_vector_setb);
    minim_load_builtin(env, "vector->list", MINIM_OBJ_FUNC, minim_builtin_vector_to_list);
    minim_load_builtin(env, "list->vector", MINIM_OBJ_FUNC, minim_builtin_list_to_vector);
    minim_load_builtin(env, "vector-fill!", MINIM_OBJ_FUNC, minim_builtin_vector_fillb);

    // Sequence
    minim_load_builtin(env, "sequence", MINIM_OBJ_FUNC, minim_builtin_sequence);
    minim_load_builtin(env, "sequence?", MINIM_OBJ_FUNC, minim_builtin_sequencep);
    minim_load_builtin(env, "sequence-first", MINIM_OBJ_FUNC, minim_builtin_sequence_first);
    minim_load_builtin(env, "sequence-rest", MINIM_OBJ_FUNC, minim_builtin_sequence_rest);
    minim_load_builtin(env, "sequence-empty?", MINIM_OBJ_FUNC, minim_builtin_sequence_donep);
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

    // Promises
    minim_load_builtin(env, "delay", MINIM_OBJ_SYNTAX, minim_builtin_delay);
    minim_load_builtin(env, "force", MINIM_OBJ_FUNC, minim_builtin_force);
    minim_load_builtin(env, "promise?", MINIM_OBJ_FUNC, minim_builtin_promisep);
}

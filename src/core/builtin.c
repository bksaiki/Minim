#include <math.h>
#include <stdarg.h>

#include "builtin.h"
#include "global.h"
#include "intern.h"
#include "number.h"

void minim_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    switch (type)
    {
    case MINIM_OBJ_FUNC:
        obj = minim_builtin(va_arg(rest, MinimBuiltin));
        break;

    case MINIM_OBJ_SYNTAX:
        obj = minim_syntax(va_arg(rest, MinimBuiltin));
        break;

    case MINIM_OBJ_PAIR:
        obj = minim_cons(va_arg(rest, MinimObject*), va_arg(rest, MinimObject*));
        break;

    case MINIM_OBJ_INEXACT:
        obj = minim_inexactnum(va_arg(rest, double));
        break;
    
    default:
        printf("Load builtin: unknown type for '%s'", name);
        return;
    }
    va_end(rest);

    env_intern_sym(env, name, obj);
}

static void init_builtin(const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    switch (type)
    {
    case MINIM_OBJ_FUNC:
        obj = minim_builtin(va_arg(rest, MinimBuiltin));
        break;

    case MINIM_OBJ_SYNTAX:
        obj = minim_syntax(va_arg(rest, MinimBuiltin));
        break;

    case MINIM_OBJ_PAIR:
        obj = minim_cons(va_arg(rest, MinimObject*), va_arg(rest, MinimObject*));
        break;

    case MINIM_OBJ_INEXACT:
        obj = minim_inexactnum(va_arg(rest, double));
        break;
    
    default:
        printf("Load builtin: unknown type for '%s'", name);
        return;
    }

    intern(name);
    set_builtin(name, obj);
    va_end(rest);
}

void init_builtins()
{
    // Variable / Control
    init_builtin("def-values", MINIM_OBJ_SYNTAX, minim_builtin_def_values);
    init_builtin("set!", MINIM_OBJ_SYNTAX, minim_builtin_setb);
    init_builtin("if", MINIM_OBJ_SYNTAX, minim_builtin_if);
    init_builtin("let-values", MINIM_OBJ_SYNTAX, minim_builtin_let_values);
    init_builtin("let*-values", MINIM_OBJ_SYNTAX, minim_builtin_letstar_values);
    init_builtin("begin", MINIM_OBJ_SYNTAX, minim_builtin_begin);
    init_builtin("quote", MINIM_OBJ_SYNTAX, minim_builtin_quote);
    init_builtin("quasiquote", MINIM_OBJ_SYNTAX, minim_builtin_quasiquote);
    init_builtin("unquote", MINIM_OBJ_SYNTAX, minim_builtin_unquote);
    init_builtin("lambda", MINIM_OBJ_SYNTAX, minim_builtin_lambda);
    init_builtin("exit", MINIM_OBJ_FUNC, minim_builtin_exit);
    init_builtin("call/cc", MINIM_OBJ_SYNTAX, minim_builtin_callcc);

    // Modules
    init_builtin("%export", MINIM_OBJ_SYNTAX, minim_builtin_export);
    init_builtin("%import", MINIM_OBJ_SYNTAX, minim_builtin_import);

    // Transforms
    init_builtin("def-syntaxes", MINIM_OBJ_SYNTAX, minim_builtin_def_syntaxes);
    init_builtin("quote-syntax", MINIM_OBJ_SYNTAX, minim_builtin_syntax);
    init_builtin("syntax?", MINIM_OBJ_FUNC, minim_builtin_syntaxp);
    init_builtin("unwrap", MINIM_OBJ_FUNC, minim_builtin_unwrap);
    init_builtin("syntax-case", MINIM_OBJ_SYNTAX, minim_builtin_syntax_case);
    init_builtin("datum->syntax", MINIM_OBJ_FUNC, minim_builtin_to_syntax);
    init_builtin("syntax", MINIM_OBJ_SYNTAX, minim_builtin_template);

    // Errors
    init_builtin("error", MINIM_OBJ_FUNC, minim_builtin_error);
    init_builtin("argument-error", MINIM_OBJ_FUNC, minim_builtin_argument_error);
    init_builtin("arity-error", MINIM_OBJ_FUNC, minim_builtin_arity_error);
    init_builtin("syntax-error", MINIM_OBJ_FUNC, minim_builtin_syntax_error);
    
    // Miscellaneous
    init_builtin("values", MINIM_OBJ_FUNC, minim_builtin_values);
    init_builtin("equal?", MINIM_OBJ_FUNC, minim_builtin_equalp);
    init_builtin("eqv?", MINIM_OBJ_FUNC, minim_builtin_eqvp);
    init_builtin("eq?", MINIM_OBJ_FUNC, minim_builtin_eqp);
    init_builtin("symbol?", MINIM_OBJ_FUNC, minim_builtin_symbolp);
    init_builtin("printf", MINIM_OBJ_FUNC, minim_builtin_printf);
    init_builtin("void", MINIM_OBJ_FUNC, minim_builtin_void);
    init_builtin("version", MINIM_OBJ_FUNC, minim_builtin_version);
    init_builtin("symbol-count", MINIM_OBJ_FUNC, minim_builtin_symbol_count);
    init_builtin("dump-symbols", MINIM_OBJ_FUNC, minim_builtin_dump_symbols);
    init_builtin("def-print-method", MINIM_OBJ_FUNC, minim_builtin_def_print_method);

    // Arity
    init_builtin("procedure?", MINIM_OBJ_FUNC, minim_builtin_procedurep);
    init_builtin("procedure-arity", MINIM_OBJ_FUNC, minim_builtin_procedure_arity);

    // Boolean
    set_builtin("true", minim_true);
    set_builtin("false", minim_false);
    init_builtin("bool?", MINIM_OBJ_FUNC, minim_builtin_boolp);
    init_builtin("not", MINIM_OBJ_FUNC, minim_builtin_not);

    // Numbers
    init_builtin("number?", MINIM_OBJ_FUNC, minim_builtin_numberp);
    init_builtin("zero?", MINIM_OBJ_FUNC, minim_builtin_zerop);
    init_builtin("positive?", MINIM_OBJ_FUNC, minim_builtin_positivep);
    init_builtin("negative?", MINIM_OBJ_FUNC, minim_builtin_negativep);
    init_builtin("even?", MINIM_OBJ_FUNC, minim_builtin_evenp);
    init_builtin("odd?", MINIM_OBJ_FUNC, minim_builtin_oddp);
    init_builtin("exact?", MINIM_OBJ_FUNC, minim_builtin_exactp);
    init_builtin("inexact?", MINIM_OBJ_FUNC, minim_builtin_inexactp);
    init_builtin("integer?", MINIM_OBJ_FUNC, minim_builtin_integerp);
    init_builtin("nan?", MINIM_OBJ_FUNC, minim_builtin_nanp);
    init_builtin("infinite?", MINIM_OBJ_FUNC, minim_builtin_infinitep);
    
    init_builtin("=", MINIM_OBJ_FUNC, minim_builtin_eq);
    init_builtin(">", MINIM_OBJ_FUNC, minim_builtin_gt);
    init_builtin("<", MINIM_OBJ_FUNC, minim_builtin_lt);
    init_builtin(">=", MINIM_OBJ_FUNC, minim_builtin_gte);
    init_builtin("<=", MINIM_OBJ_FUNC, minim_builtin_lte);

    init_builtin("exact", MINIM_OBJ_FUNC, minim_builtin_to_exact);
    init_builtin("inexact", MINIM_OBJ_FUNC, minim_builtin_to_inexact);
    
    init_builtin("inf", MINIM_OBJ_INEXACT, INFINITY);
    init_builtin("-inf", MINIM_OBJ_INEXACT, -INFINITY);
    init_builtin("nan", MINIM_OBJ_INEXACT, NAN);

    // Character
    init_builtin("char?", MINIM_OBJ_FUNC, minim_builtin_charp);
    init_builtin("char=?", MINIM_OBJ_FUNC, minim_builtin_char_eqp);
    init_builtin("char>?", MINIM_OBJ_FUNC, minim_builtin_char_gtp);
    init_builtin("char<?", MINIM_OBJ_FUNC, minim_builtin_char_ltp);
    init_builtin("char>=?", MINIM_OBJ_FUNC, minim_builtin_char_gtep);
    init_builtin("char<=?", MINIM_OBJ_FUNC, minim_builtin_char_ltep);
    init_builtin("char-ci=?", MINIM_OBJ_FUNC, minim_builtin_char_ci_eqp);
    init_builtin("char-ci>?", MINIM_OBJ_FUNC, minim_builtin_char_ci_gtp);
    init_builtin("char-ci<?", MINIM_OBJ_FUNC, minim_builtin_char_ci_ltp);
    init_builtin("char-ci>=?", MINIM_OBJ_FUNC, minim_builtin_char_ci_gtep);
    init_builtin("char-ci<=?", MINIM_OBJ_FUNC, minim_builtin_char_ci_ltep);
    init_builtin("char-alphabetic?", MINIM_OBJ_FUNC, minim_builtin_char_alphabeticp);
    init_builtin("char-numeric?", MINIM_OBJ_FUNC, minim_builtin_char_numericp);
    init_builtin("char-whitespace?", MINIM_OBJ_FUNC, minim_builtin_char_whitespacep);
    init_builtin("char-upper-case?", MINIM_OBJ_FUNC, minim_builtin_char_upper_casep);
    init_builtin("char-lower-case?", MINIM_OBJ_FUNC, minim_builtin_char_lower_casep);
    init_builtin("integer->char", MINIM_OBJ_FUNC, minim_builtin_int_to_char);
    init_builtin("char->integer", MINIM_OBJ_FUNC, minim_builtin_char_to_int);
    init_builtin("char-upcase", MINIM_OBJ_FUNC, minim_builtin_char_upcase);
    init_builtin("char-downcase", MINIM_OBJ_FUNC, minim_builtin_char_downcase);

    // String
    init_builtin("string?", MINIM_OBJ_FUNC, minim_builtin_stringp);
    init_builtin("make-string", MINIM_OBJ_FUNC, minim_builtin_make_string);
    init_builtin("string", MINIM_OBJ_FUNC, minim_builtin_string);
    init_builtin("string-length", MINIM_OBJ_FUNC, minim_builtin_string_length);
    init_builtin("string-ref", MINIM_OBJ_FUNC, minim_builtin_string_ref);
    init_builtin("string-set!", MINIM_OBJ_FUNC, minim_builtin_string_setb);
    init_builtin("string-copy", MINIM_OBJ_FUNC, minim_builtin_string_copy);
    init_builtin("string-fill!", MINIM_OBJ_FUNC, minim_builtin_string_fillb);
    init_builtin("substring", MINIM_OBJ_FUNC, minim_builtin_substring);
    init_builtin("string->symbol", MINIM_OBJ_FUNC, minim_builtin_string_to_symbol);
    init_builtin("symbol->string", MINIM_OBJ_FUNC, minim_builtin_symbol_to_string);
    init_builtin("string->number", MINIM_OBJ_FUNC, minim_builtin_string_to_number);
    init_builtin("number->string", MINIM_OBJ_FUNC, minim_builtin_number_to_string);
    init_builtin("format", MINIM_OBJ_FUNC, minim_builtin_format);
    
    // Pair
    init_builtin("cons", MINIM_OBJ_FUNC, minim_builtin_cons);
    init_builtin("pair?", MINIM_OBJ_FUNC, minim_builtin_consp);
    init_builtin("car", MINIM_OBJ_FUNC, minim_builtin_car);
    init_builtin("cdr", MINIM_OBJ_FUNC, minim_builtin_cdr);
    set_builtin("null", minim_null);

    init_builtin("caar", MINIM_OBJ_FUNC, minim_builtin_caar);
    init_builtin("cadr", MINIM_OBJ_FUNC, minim_builtin_cadr);
    init_builtin("cdar", MINIM_OBJ_FUNC, minim_builtin_cdar);
    init_builtin("cddr", MINIM_OBJ_FUNC, minim_builtin_cddr);

    init_builtin("caaar", MINIM_OBJ_FUNC, minim_builtin_caaar);
    init_builtin("caadr", MINIM_OBJ_FUNC, minim_builtin_caadr);
    init_builtin("cadar", MINIM_OBJ_FUNC, minim_builtin_cadar);
    init_builtin("caddr", MINIM_OBJ_FUNC, minim_builtin_caddr);
    init_builtin("cdaar", MINIM_OBJ_FUNC, minim_builtin_cdaar);
    init_builtin("cdadr", MINIM_OBJ_FUNC, minim_builtin_cdadr);
    init_builtin("cddar", MINIM_OBJ_FUNC, minim_builtin_cddar);
    init_builtin("cdddr", MINIM_OBJ_FUNC, minim_builtin_cdddr);

    init_builtin("caaaar", MINIM_OBJ_FUNC, minim_builtin_caaaar);
    init_builtin("caaadr", MINIM_OBJ_FUNC, minim_builtin_caaadr);
    init_builtin("caadar", MINIM_OBJ_FUNC, minim_builtin_caadar);
    init_builtin("caaddr", MINIM_OBJ_FUNC, minim_builtin_caaddr);
    init_builtin("cadaar", MINIM_OBJ_FUNC, minim_builtin_cadaar);
    init_builtin("cadadr", MINIM_OBJ_FUNC, minim_builtin_cadadr);
    init_builtin("caddar", MINIM_OBJ_FUNC, minim_builtin_caddar);
    init_builtin("cadddr", MINIM_OBJ_FUNC, minim_builtin_cadddr);
    init_builtin("cdaaar", MINIM_OBJ_FUNC, minim_builtin_cdaaar);
    init_builtin("cdaadr", MINIM_OBJ_FUNC, minim_builtin_cdaadr);
    init_builtin("cdadar", MINIM_OBJ_FUNC, minim_builtin_cdadar);
    init_builtin("cdaddr", MINIM_OBJ_FUNC, minim_builtin_cdaddr);
    init_builtin("cddaar", MINIM_OBJ_FUNC, minim_builtin_cddaar);
    init_builtin("cddadr", MINIM_OBJ_FUNC, minim_builtin_cddadr);
    init_builtin("cdddar", MINIM_OBJ_FUNC, minim_builtin_cdddar);
    init_builtin("cddddr", MINIM_OBJ_FUNC, minim_builtin_cddddr);

    // List
    init_builtin("list", MINIM_OBJ_FUNC, minim_builtin_list);
    init_builtin("list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    init_builtin("null?", MINIM_OBJ_FUNC, minim_builtin_nullp);
    init_builtin("length", MINIM_OBJ_FUNC, minim_builtin_length);
    init_builtin("append", MINIM_OBJ_FUNC, minim_builtin_append);
    init_builtin("reverse", MINIM_OBJ_FUNC, minim_builtin_reverse);
    init_builtin("list-ref", MINIM_OBJ_FUNC, minim_builtin_list_ref);
    init_builtin("map", MINIM_OBJ_FUNC, minim_builtin_map);
    init_builtin("andmap", MINIM_OBJ_FUNC, minim_builtin_andmap);
    init_builtin("ormap", MINIM_OBJ_FUNC, minim_builtin_ormap);
    init_builtin("apply", MINIM_OBJ_FUNC, minim_builtin_apply);

    // Hash table
    init_builtin("hash", MINIM_OBJ_FUNC, minim_builtin_hash);
    init_builtin("hash?", MINIM_OBJ_FUNC, minim_builtin_hashp);
    init_builtin("hash-key?", MINIM_OBJ_FUNC, minim_builtin_hash_keyp);
    init_builtin("hash-ref", MINIM_OBJ_FUNC, minim_builtin_hash_ref);
    init_builtin("hash-remove", MINIM_OBJ_FUNC, minim_builtin_hash_remove);
    init_builtin("hash-set", MINIM_OBJ_FUNC, minim_builtin_hash_set);
    init_builtin("hash-set!", MINIM_OBJ_FUNC, minim_builtin_hash_setb);
    init_builtin("hash-remove!", MINIM_OBJ_FUNC, minim_builtin_hash_removeb);
    init_builtin("hash->list", MINIM_OBJ_FUNC, minim_builtin_hash_to_list);

    // Vector
    init_builtin("vector", MINIM_OBJ_FUNC, minim_builtin_vector);
    init_builtin("make-vector", MINIM_OBJ_FUNC, minim_builtin_make_vector);
    init_builtin("vector?", MINIM_OBJ_FUNC, minim_builtin_vectorp);
    init_builtin("vector-length", MINIM_OBJ_FUNC, minim_builtin_vector_length);
    init_builtin("vector-ref", MINIM_OBJ_FUNC, minim_builtin_vector_ref);
    init_builtin("vector-set!", MINIM_OBJ_FUNC, minim_builtin_vector_setb);
    init_builtin("vector->list", MINIM_OBJ_FUNC, minim_builtin_vector_to_list);
    init_builtin("list->vector", MINIM_OBJ_FUNC, minim_builtin_list_to_vector);
    init_builtin("vector-fill!", MINIM_OBJ_FUNC, minim_builtin_vector_fillb);

    // Sequence
    init_builtin("sequence", MINIM_OBJ_FUNC, minim_builtin_sequence);
    init_builtin("sequence?", MINIM_OBJ_FUNC, minim_builtin_sequencep);
    init_builtin("sequence-first", MINIM_OBJ_FUNC, minim_builtin_sequence_first);
    init_builtin("sequence-rest", MINIM_OBJ_FUNC, minim_builtin_sequence_rest);
    init_builtin("sequence-empty?", MINIM_OBJ_FUNC, minim_builtin_sequence_donep);

    // Math
    init_builtin("+", MINIM_OBJ_FUNC, minim_builtin_add);
    init_builtin("-", MINIM_OBJ_FUNC, minim_builtin_sub);
    init_builtin("*", MINIM_OBJ_FUNC, minim_builtin_mul);
    init_builtin("/", MINIM_OBJ_FUNC, minim_builtin_div);
    init_builtin("abs", MINIM_OBJ_FUNC, minim_builtin_abs);
    init_builtin("max", MINIM_OBJ_FUNC, minim_builtin_max);
    init_builtin("min", MINIM_OBJ_FUNC, minim_builtin_min);
    init_builtin("modulo", MINIM_OBJ_FUNC, minim_builtin_modulo);
    init_builtin("remainder", MINIM_OBJ_FUNC, minim_builtin_remainder);
    init_builtin("quotient", MINIM_OBJ_FUNC, minim_builtin_quotient);
    init_builtin("numerator", MINIM_OBJ_FUNC, minim_builtin_numerator);
    init_builtin("denominator", MINIM_OBJ_FUNC, minim_builtin_denominator);
    init_builtin("gcd", MINIM_OBJ_FUNC, minim_builtin_gcd);
    init_builtin("lcm", MINIM_OBJ_FUNC, minim_builtin_lcm);

    init_builtin("floor", MINIM_OBJ_FUNC, minim_builtin_floor);
    init_builtin("ceil", MINIM_OBJ_FUNC, minim_builtin_ceil);
    init_builtin("trunc", MINIM_OBJ_FUNC, minim_builtin_trunc);
    init_builtin("round", MINIM_OBJ_FUNC, minim_builtin_round);

    init_builtin("sqrt", MINIM_OBJ_FUNC, minim_builtin_sqrt);
    init_builtin("exp", MINIM_OBJ_FUNC, minim_builtin_exp);
    init_builtin("log", MINIM_OBJ_FUNC, minim_builtin_log);
    init_builtin("pow", MINIM_OBJ_FUNC, minim_builtin_pow);

    init_builtin("sin", MINIM_OBJ_FUNC, minim_builtin_sin);
    init_builtin("cos", MINIM_OBJ_FUNC, minim_builtin_cos);
    init_builtin("tan", MINIM_OBJ_FUNC, minim_builtin_tan);
    init_builtin("asin", MINIM_OBJ_FUNC, minim_builtin_asin);
    init_builtin("acos", MINIM_OBJ_FUNC, minim_builtin_acos);
    init_builtin("atan", MINIM_OBJ_FUNC, minim_builtin_atan);

    // Promises
    init_builtin("delay", MINIM_OBJ_SYNTAX, minim_builtin_delay);
    init_builtin("force", MINIM_OBJ_FUNC, minim_builtin_force);
    init_builtin("promise?", MINIM_OBJ_FUNC, minim_builtin_promisep);

    // Port
    init_builtin("current-input-port", MINIM_OBJ_FUNC, minim_builtin_current_input_port);
    init_builtin("current-output-port", MINIM_OBJ_FUNC, minim_builtin_current_output_port);
    init_builtin("call-with-input-file", MINIM_OBJ_FUNC, minim_builtin_call_with_input_file);
    init_builtin("call-with-output-file", MINIM_OBJ_FUNC, minim_builtin_call_with_output_file);
    init_builtin("with-input-from-file", MINIM_OBJ_FUNC, minim_builtin_with_input_from_file);
    init_builtin("with-output-from-file", MINIM_OBJ_FUNC, minim_builtin_with_output_from_file);
    init_builtin("port?", MINIM_OBJ_FUNC, minim_builtin_portp);
    init_builtin("input-port?", MINIM_OBJ_FUNC, minim_builtin_input_portp);
    init_builtin("output-port?", MINIM_OBJ_FUNC, minim_builtin_output_portp);
    init_builtin("open-input-file", MINIM_OBJ_FUNC, minim_builtin_open_input_file);
    init_builtin("open-output-file", MINIM_OBJ_FUNC, minim_builtin_open_output_file);
    init_builtin("close-input-port", MINIM_OBJ_FUNC, minim_builtin_close_input_port);
    init_builtin("close-output-port", MINIM_OBJ_FUNC, minim_builtin_close_output_port);

    set_builtin("eof", minim_eof);
    init_builtin("eof?", MINIM_OBJ_FUNC, minim_builtin_eofp);
    init_builtin("read", MINIM_OBJ_FUNC, minim_builtin_read);
    init_builtin("read-char", MINIM_OBJ_FUNC, minim_builtin_read_char);
    init_builtin("peek-char", MINIM_OBJ_FUNC, minim_builtin_peek_char);
    init_builtin("char-ready?", MINIM_OBJ_FUNC, minim_builtin_char_readyp);

    init_builtin("write", MINIM_OBJ_FUNC, minim_builtin_write);
    init_builtin("display", MINIM_OBJ_FUNC, minim_builtin_display);
    init_builtin("newline", MINIM_OBJ_FUNC, minim_builtin_newline);
    init_builtin("write-char", MINIM_OBJ_FUNC, minim_builtin_write_char);
}

#include <math.h>
#include "minimpriv.h"

#define MAX_ARGC        32768

#define init_prim_closure_variary(n, fn, mina, maxa)    \
    init_builtin(n, MINIM_OBJ_PRIM_CLOSURE, minim_builtin_ ## fn, (size_t) mina, (size_t) maxa)

#define init_syntax_form(n, fn)     \
    init_builtin(n, MINIM_OBJ_SYNTAX, minim_builtin_ ## fn)

#define init_prim_closure_exact(n, fn, a)       init_prim_closure_variary(n, fn, a, a)
#define init_prim_closure_min(n, fn, a)         init_prim_closure_variary(n, fn, a, MAX_ARGC)
#define init_prim_closure_no_check(n, fn)       init_prim_closure_variary(n, fn, 0, MAX_ARGC)

static void init_builtin(const char *name, MinimObjectType type, ...)
{
    MinimObject *obj, *isym;
    MinimPrimClosureFn fn;
    size_t min_argc, max_argc;
    va_list rest;

    isym = intern(name);
    va_start(rest, type);
    switch (type)
    {
    case MINIM_OBJ_PRIM_CLOSURE:
        fn = va_arg(rest, MinimPrimClosureFn);
        min_argc = va_arg(rest, size_t);
        max_argc = va_arg(rest, size_t);
        obj = minim_prim_closure(fn, MINIM_SYMBOL(isym), min_argc, max_argc);
        break;

    case MINIM_OBJ_SYNTAX:
        obj = minim_syntax(va_arg(rest, MinimPrimClosureFn));
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

    set_builtin(MINIM_SYMBOL(isym), obj);
    va_end(rest);
}

static void init_syntax_forms()
{
    // Variable / Control
    init_syntax_form("def-values", def_values);
    init_syntax_form("set!", setb);
    init_syntax_form("if", if);
    init_syntax_form("let-values", let_values);
    init_syntax_form("begin", begin);
    init_syntax_form("quote", quote);
    init_syntax_form("quasiquote", quasiquote);
    init_syntax_form("unquote", unquote);
    init_syntax_form("lambda", lambda);
    init_syntax_form("call/cc", callcc);

    // Modules
    init_syntax_form("%export", export);
    init_syntax_form("%import", import);

    // Transform
    init_syntax_form("def-syntaxes", def_syntaxes);
    init_syntax_form("quote-syntax", syntax);
    init_syntax_form("syntax-case", syntax_case);
    init_syntax_form("syntax", template);
    init_syntax_form("identifier=?", identifier_equalp);        // TODO: is this really syntax?

    // Promise
    init_syntax_form("delay", delay);
}

void init_builtins()
{
    // Syntax
    init_syntax_forms();

    // Variable / Control
    init_prim_closure_variary("exit", exit, 0, 1);

    // Transforms
    init_prim_closure_exact("syntax?", syntaxp, 1);
    init_prim_closure_exact("datum->syntax", to_syntax, 1);
    init_prim_closure_exact("unwrap", unwrap, 1);
    
    // Errors
    init_prim_closure_min("error", error, 1);
    init_prim_closure_min("argument-error", argument_error, 3);
    init_prim_closure_min("arity-error", arity_error, 2);
    init_prim_closure_variary("syntax-error", syntax_error, 2, 4);
    
    // Miscellaneous
    init_prim_closure_no_check("values", values);
    init_prim_closure_exact("equal?", equalp, 2);
    init_prim_closure_exact("eqv?", eqvp, 2);
    init_prim_closure_exact("eq?", eqp, 2);
    init_prim_closure_exact("symbol?", symbolp, 1);
    init_prim_closure_min("printf", printf, 1);
    init_prim_closure_exact("void", void, 0);
    init_prim_closure_exact("immutable?", immutablep, 1);
    init_prim_closure_exact("version", version, 0);
    init_prim_closure_exact("symbol-count", symbol_count, 0);
    init_prim_closure_exact("dump-symbols", dump_symbols, 0);
    init_prim_closure_exact("def-print-method", def_print_method, 2);

    // Arity
    init_prim_closure_exact("procedure?", procedurep, 1);
    init_prim_closure_exact("procedure-arity", procedure_arity, 1);

    // Boolean
    set_builtin("true", minim_true);
    set_builtin("false", minim_false);
    init_prim_closure_exact("bool?", boolp, 1);
    init_prim_closure_exact("not", not, 1);

    // Numbers
    init_prim_closure_exact("number?", numberp, 1);
    init_prim_closure_exact("zero?", zerop, 1);
    init_prim_closure_exact("positive?", positivep, 1);
    init_prim_closure_exact("negative?", negativep, 1);
    init_prim_closure_exact("even?", evenp, 1);
    init_prim_closure_exact("odd?", oddp, 1);
    init_prim_closure_exact("exact?", exactp, 1);
    init_prim_closure_exact("inexact?", inexactp, 1);
    init_prim_closure_exact("integer?", integerp, 1);
    init_prim_closure_exact("nan?", nanp, 1);
    init_prim_closure_exact("infinite?", infinitep, 1);
    
    init_prim_closure_min("=", eq, 1);
    init_prim_closure_min(">", gt, 1);
    init_prim_closure_min("<", lt, 1);
    init_prim_closure_min(">=", gte, 1);
    init_prim_closure_min("<=", lte, 1);

    init_prim_closure_exact("exact", to_exact, 1);
    init_prim_closure_exact("inexact", to_inexact, 1);
    
    init_builtin("inf", MINIM_OBJ_INEXACT, INFINITY);
    init_builtin("-inf", MINIM_OBJ_INEXACT, -INFINITY);
    init_builtin("nan", MINIM_OBJ_INEXACT, NAN);

    // Character
    init_prim_closure_exact("char?", charp, 1);
    init_prim_closure_exact("char=?", char_eqp, 2);
    init_prim_closure_exact("char>?", char_gtp, 2);
    init_prim_closure_exact("char<?", char_ltp, 2);
    init_prim_closure_exact("char>=?", char_gtep, 2);
    init_prim_closure_exact("char<=?", char_ltep, 2);
    init_prim_closure_exact("char-ci=?", char_ci_eqp, 2);
    init_prim_closure_exact("char-ci>?", char_ci_gtp, 2);
    init_prim_closure_exact("char-ci<?", char_ci_ltp, 2);
    init_prim_closure_exact("char-ci>=?", char_ci_gtep, 2);
    init_prim_closure_exact("char-ci<=?", char_ci_ltep, 2);
    init_prim_closure_exact("char-alphabetic?", char_alphabeticp, 1);
    init_prim_closure_exact("char-numeric?", char_numericp, 1);
    init_prim_closure_exact("char-whitespace?", char_whitespacep, 1);
    init_prim_closure_exact("char-upper-case?", char_upper_casep, 1);
    init_prim_closure_exact("char-lower-case?", char_lower_casep, 1);
    init_prim_closure_exact("integer->char", int_to_char, 1);
    init_prim_closure_exact("char->integer", char_to_int, 1);
    init_prim_closure_exact("char-upcase", char_upcase, 1);
    init_prim_closure_exact("char-downcase", char_downcase, 1);

    // String
    init_prim_closure_exact("string?", stringp, 1);
    init_prim_closure_variary("make-string", make_string, 1, 2);
    init_prim_closure_no_check("string", string);
    init_prim_closure_exact("string-length", string_length, 1);
    init_prim_closure_exact("string-ref", string_ref, 2);
    init_prim_closure_exact("string-set!", string_setb, 3);
    init_prim_closure_exact("string-copy", string_copy, 1);
    init_prim_closure_exact("string-fill!", string_fillb, 2);
    init_prim_closure_no_check("string-append", string_append);
    init_prim_closure_variary("substring", substring, 2, 3);
    init_prim_closure_exact("string->symbol", string_to_symbol, 1);
    init_prim_closure_exact("symbol->string", symbol_to_string, 1);
    init_prim_closure_exact("string->number", string_to_number, 1);
    init_prim_closure_exact("number->string", number_to_string, 1);
    init_prim_closure_min("format", format, 1);
    
    // Pair
    init_prim_closure_exact("cons", cons, 1);
    init_prim_closure_exact("pair?", consp, 1);
    init_prim_closure_exact("car", car, 1);
    init_prim_closure_exact("cdr", cdr, 1);
    set_builtin("null", minim_null);

    init_prim_closure_exact("caar", caar, 1);
    init_prim_closure_exact("cadr", cadr, 1);
    init_prim_closure_exact("cdar", cdar, 1);
    init_prim_closure_exact("cddr", cddr, 1);

    init_prim_closure_exact("caaar", caaar, 1);
    init_prim_closure_exact("caadr", caadr, 1);
    init_prim_closure_exact("cadar", cadar, 1);
    init_prim_closure_exact("caddr", caddr, 1);
    init_prim_closure_exact("cdaar", cdaar, 1);
    init_prim_closure_exact("cdadr", cdadr, 1);
    init_prim_closure_exact("cddar", cddar, 1);
    init_prim_closure_exact("cdddr", cdddr, 1);

    init_prim_closure_exact("caaaar", caaaar, 1);
    init_prim_closure_exact("caaadr", caaadr, 1);
    init_prim_closure_exact("caadar", caadar, 1);
    init_prim_closure_exact("caaddr", caaddr, 1);
    init_prim_closure_exact("cadaar", cadaar, 1);
    init_prim_closure_exact("cadadr", cadadr, 1);
    init_prim_closure_exact("caddar", caddar, 1);
    init_prim_closure_exact("cadddr", cadddr, 1);
    init_prim_closure_exact("cdaaar", cdaaar, 1);
    init_prim_closure_exact("cdaadr", cdaadr, 1);
    init_prim_closure_exact("cdadar", cdadar, 1);
    init_prim_closure_exact("cdaddr", cdaddr, 1);
    init_prim_closure_exact("cddaar", cddaar, 1);
    init_prim_closure_exact("cddadr", cddadr, 1);
    init_prim_closure_exact("cdddar", cdddar, 1);
    init_prim_closure_exact("cddddr", cddddr, 1);

    // List
    init_prim_closure_no_check("list", list);
    init_prim_closure_exact("list?", listp, 1);
    init_prim_closure_exact("null?", nullp, 1);
    init_prim_closure_exact("length", length, 1);
    init_prim_closure_min("append", append, 1);
    init_prim_closure_exact("reverse", reverse, 1);
    init_prim_closure_exact("list-ref", list_ref, 2);
    init_prim_closure_min("map", map, 2);
    init_prim_closure_min("andmap", andmap, 2);
    init_prim_closure_min("ormap", ormap, 2);
    init_prim_closure_min("apply", apply, 2);

    // Hash table
    init_prim_closure_exact("hash", hash, 0);
    init_prim_closure_exact("hash?", hashp, 1);
    init_prim_closure_exact("hash-key?", hash_keyp, 2);
    init_prim_closure_exact("hash-ref", hash_ref, 2);
    init_prim_closure_exact("hash-remove", hash_remove, 2);
    init_prim_closure_exact("hash-set", hash_set, 3);
    init_prim_closure_exact("hash-set!", hash_setb, 3);
    init_prim_closure_exact("hash-remove!", hash_removeb, 2);
    init_prim_closure_exact("hash->list", hash_to_list, 1);

    // Vector
    init_prim_closure_no_check("vector", vector);
    init_prim_closure_variary("make-vector", make_vector, 1, 2);
    init_prim_closure_exact("vector?", vectorp, 1);
    init_prim_closure_exact("vector-length", vector_length, 1);
    init_prim_closure_exact("vector-ref", vector_ref, 2);
    init_prim_closure_exact("vector-set!", vector_setb, 3);
    init_prim_closure_exact("vector->list", vector_to_list, 1);
    init_prim_closure_exact("list->vector", list_to_vector, 1);
    init_prim_closure_exact("vector-fill!", vector_fillb, 2);

    // Math
    init_prim_closure_no_check("+", add);
    init_prim_closure_min("-", sub, 1);
    init_prim_closure_no_check("*", mul);
    init_prim_closure_min("/", div, 1);
    init_prim_closure_min("max", max, 1);
    init_prim_closure_min("min", min, 1);
    init_prim_closure_exact("abs", abs, 1);
    init_prim_closure_exact("modulo", modulo, 2);
    init_prim_closure_exact("remainder", remainder, 2);
    init_prim_closure_exact("quotient", quotient, 2);
    init_prim_closure_exact("numerator", numerator, 1);
    init_prim_closure_exact("denominator", denominator, 1);
    init_prim_closure_exact("gcd", gcd, 2);
    init_prim_closure_exact("lcm", lcm, 2);

    init_prim_closure_exact("floor", floor, 1);
    init_prim_closure_exact("ceil", ceil, 1);
    init_prim_closure_exact("trunc", trunc, 1);
    init_prim_closure_exact("round", round, 1);

    init_prim_closure_exact("sqrt", sqrt, 1);
    init_prim_closure_exact("exp", exp, 1);
    init_prim_closure_exact("log", log, 1);
    init_prim_closure_exact("pow", pow, 2);

    init_prim_closure_exact("sin", sin, 1);
    init_prim_closure_exact("cos", cos, 1);
    init_prim_closure_exact("tan", tan, 1);
    init_prim_closure_exact("asin", asin, 1);
    init_prim_closure_exact("acos", acos, 1);
    init_prim_closure_exact("atan", atan, 1);

    // Promises
    init_prim_closure_exact("force", force, 1);
    init_prim_closure_exact("promise?", promisep, 1);

    // Records
    init_prim_closure_variary("make-record", make_record, 1, 2);
    init_prim_closure_min("record", record, 1);
    init_prim_closure_exact("record?", recordp, 1);
    init_prim_closure_exact("record-length", record_length, 1);
    init_prim_closure_exact("record-type", record_type, 1);
    init_prim_closure_exact("record-ref", record_ref, 2);
    init_prim_closure_exact("record-set!", record_setb, 3);
    init_prim_closure_exact("record-set-type!", record_set_typeb, 2);

    // Port
    init_prim_closure_exact("current-input-port", current_input_port, 0);
    init_prim_closure_exact("current-output-port", current_output_port, 0);
    init_prim_closure_exact("call-with-input-file", call_with_input_file, 2);
    init_prim_closure_exact("call-with-output-file", call_with_output_file, 2);
    init_prim_closure_exact("with-input-from-file", with_input_from_file, 2);
    init_prim_closure_exact("with-output-from-file", with_output_from_file, 2);
    init_prim_closure_exact("port?", portp, 1);
    init_prim_closure_exact("input-port?", input_portp, 1);
    init_prim_closure_exact("output-port?", output_portp, 1);
    init_prim_closure_exact("open-input-file", open_input_file, 1);
    init_prim_closure_exact("open-output-file", open_output_file, 1);
    init_prim_closure_exact("close-input-port", close_input_port, 1);
    init_prim_closure_exact("close-output-port", close_output_port, 1);

    set_builtin("eof", minim_eof);
    init_prim_closure_exact("eof?", eofp, 1);
    init_prim_closure_variary("read", read, 0, 1);
    init_prim_closure_variary("read-char", read_char, 0, 1);
    init_prim_closure_variary("peek-char", peek_char, 0, 1);
    init_prim_closure_variary("char-ready?", char_readyp, 0, 1);

    init_prim_closure_variary("write", write, 1, 2);
    init_prim_closure_variary("display", display, 1, 2);
    init_prim_closure_variary("newline", newline, 0, 1);
    init_prim_closure_variary("write-char", write_char, 1, 2);
}

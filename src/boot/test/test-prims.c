/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations

    Tests for the primitive library
*/

#include "../c/boot.h"

FILE *stream;
minim_object *env;
int return_code, passed;

#define reset() {               \
    rewind(stream);             \
    env = make_env();    \
}

#define load(s) {       \
    fputs(s, stream);   \
    rewind(stream);     \
}

#define eval(s)     eval_expr(read_object(stream), env)

char *write(minim_object *o) {
    char *str;
    size_t read;
    long length;

    rewind(stream);
    write_object(stream, o);

    length = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    str = GC_alloc_atomic((length + 1) * sizeof(char));
    read = fread(str, 1, length, stream);
    str[length] = '\0';

    if (read != length) {
        fprintf(stderr, "read error occured");
        exit(1);
    }

    return str;
}

#define no_check(s) {       \
    reset();                \
    load(s);                \
    eval();                 \
}

#define log_failed_case(s, expect, actual) {                        \
    printf(" %s => expected: %s, actual: %s\n", s, expect, actual); \
}

void check_true(const char *input) {
    minim_object *result;

    reset();
    load(input);
    result = eval();
    if (!minim_is_true(result)) {
        log_failed_case(input, "#t", write(result));
    }
}

void check_false(const char *input) {
    minim_object *result;

    reset();
    load(input);
    result = eval();
    if (!minim_is_false(result)) {
        log_failed_case(input, "#f", write(result));
    }
}

void check_equal(const char *input, const char *expect) {
    char *str;

    reset();
    load(input);
    str = write(eval());
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
    }
    
}

#define log_test(name, t) {             \
    if (t() == 1) {                     \
        printf("[PASSED] %s\n", name);  \
    } else {                            \
        return_code = 1;                \
        printf("[FAILED] %s\n", name);  \
    }                                   \
}

int test_simple_eval() {
    passed = 1;

    no_check("1");
    no_check("'a");
    no_check("#\\a");
    no_check("\"foo\"");
    no_check("'()");
    no_check("'(1 . 2)");
    no_check("'(1 2 3)");
    no_check("'(1 2 . 3)");

    return passed;
}

int test_type_predicates() {
    passed = 1;

    check_true ("(boolean? #t)");
    check_true ("(boolean? #f)");
    check_false("(boolean? 'a)");

    check_true ("(symbol? 'a)");
    check_false("(symbol? 1)");

    check_true ("(integer? 1)");
    check_false("(integer? 'a)");

    check_true ("(char? #\\a)");
    check_false("(char? 1)");

    check_true ("(string? \"foo\")");
    check_false("(string? 1)");

    check_true ("(procedure? procedure?)");
    check_true ("(procedure? (lambda () 1))");
    check_false("(procedure? 1)");

    return passed;
}

int test_eq() {
    passed = 1;

    check_true ("(eq? #t #t)");
    check_true ("(eq? #f #f)");
    check_false("(eq? #t #f)");

    check_true ("(eq? 1 1)");
    check_false("(eq? 1 2)");

    check_true ("(eq? #\\a #\\a)");
    check_false("(eq? #\\a #\\b)");

    check_false("(eq? \"\" \"\")");
    check_false("(eq? \"a\" \"a\")");

    check_true ("(eq? '() '())");
    check_false("(eq? '(1) '(1))");
    check_false("(eq? '(1 . 2) '(1 . 2))");

    check_true ("(eq? car car)");
    check_false("(eq? car cdr)");

    check_true ("(let ((x '(a))) (eq? x x))");
    check_true ("(let ((f (lambda (x) x))) (eq? f f))");

    return passed;
}

int test_equal() {
    passed = 1;

    check_true ("(equal? #t #t)");
    check_true ("(equal? #f #f)");
    check_false("(equal? #t #f)");

    check_true ("(equal? 1 1)");
    check_false("(equal? 1 2)");

    check_true ("(equal? #\\a #\\a)");
    check_false("(equal? #\\a #\\b)");

    check_true ("(equal? \"\" \"\")");
    check_true ("(equal? \"a\" \"a\")");
    check_false("(equal? \"a\" \"b\")");
    check_true ("(equal? \"abc\" \"abc\")");

    check_true ("(equal? '() '())");
    check_true ("(equal? '(1) '(1))");
    check_true ("(equal? '(1 . 2) '(1 . 2))");
    check_false("(equal? '(1 . 2) '(1 . 3))");
    check_true ("(equal? '(1 2 3) '(1 2 3))");

    check_true ("(equal? car car)");
    check_false("(equal? car cdr)");

    check_true ("(let ((x '(a))) (equal? x x))");
    check_true ("(let ((f (lambda (x) x))) (equal? f f))");

    return passed;
}

int test_type_conversions() {
    passed = 1;

    check_equal("(char->integer #\\A)", "65");
    check_equal("(char->integer #\\a)", "97");
    check_equal("(integer->char 65)", "#\\A");
    check_equal("(integer->char 97)", "#\\a");

    check_equal("(number->string 0)", "\"0\"");
    check_equal("(number->string 100)", "\"100\"");
    check_equal("(string->number \"0\")", "0");
    check_equal("(string->number \"100\")", "100");

    check_equal("(symbol->string 'a)", "\"a\"");
    check_equal("(symbol->string 'foo)", "\"foo\"");
    check_equal("(string->symbol \"a\")", "'a");
    check_equal("(string->symbol \"foo\")", "'foo");

    return passed;
}

int test_list() {
    passed = 1;

    check_equal("(cons 1 2)", "'(1 . 2)");
    check_equal("(cons 1 (cons 2 3))", "'(1 2 . 3)");
    check_equal("(cons 1 '())", "'(1)");

    check_equal("(car '(1 . 2))", "1");
    check_equal("(cdr '(1 . 2))", "2");
    check_equal("(car '(1 . (2 . 3)))", "1");
    check_equal("(cdr '(1 . (2 . 3)))", "'(2 . 3)");
    check_equal("(car '(1))", "1");
    check_equal("(cdr '(1))", "'()");

    check_equal("(caar '((1 . 2) 3 . 4))", "1");
    check_equal("(cadr '((1 . 2) 3 . 4))", "3");
    check_equal("(cdar '((1 . 2) 3 . 4))", "2");
    check_equal("(cddr '((1 . 2) 3 . 4))", "4");

    check_equal("(caaar '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "1");
    check_equal("(caadr '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "5");
    check_equal("(cadar '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "3");
    check_equal("(caddr '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "7");
    check_equal("(cdaar '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "2");
    check_equal("(cdadr '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "6");
    check_equal("(cddar '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "4");
    check_equal("(cdddr '(((1 . 2) . (3 . 4)) (5 . 6) . (7 . 8)))", "8");

    check_equal("(caaaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "1");
    check_equal("(caaadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "9");
    check_equal("(caadar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "5");
    check_equal("(caaddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "13");
    check_equal("(cadaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "3");
    check_equal("(cadadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "11");
    check_equal("(caddar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "7");
    check_equal("(cadddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "15");
    check_equal("(cdaaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "2");
    check_equal("(cdaadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "10");
    check_equal("(cdadar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "6");
    check_equal("(cdaddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "14");
    check_equal("(cddaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "4");
    check_equal("(cddadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "12");
    check_equal("(cdddar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "8");
    check_equal("(cddddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) ((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16))))", "16");

    check_equal("(list)", "'()");
    check_equal("(list 1)", "'(1)");
    check_equal("(list 1 2)", "'(1 2)");
    check_equal("(list 1 2 3)", "'(1 2 3)");

    check_equal("(length '())", "0");
    check_equal("(length '(1))", "1");
    check_equal("(length '(1 2))", "2");
    check_equal("(length '(1 2 3))", "3");

    check_equal("(reverse '())", "'()");
    check_equal("(reverse '(1))", "'(1)");
    check_equal("(reverse '(1 2))", "'(2 1)");
    check_equal("(reverse '(1 2 3))", "'(3 2 1)");

    check_equal("(append)", "'()");
    check_equal("(append '())", "'()");
    check_equal("(append '() '() '())", "'()");
    check_equal("(append '(a))", "'(a)");
    check_equal("(append '(a b c))", "'(a b c)");
    check_equal("(append '(a b c) '(d))", "'(a b c d)");
    check_equal("(append '(a b c) '(d e f))", "'(a b c d e f)");

    check_equal("(for-each (lambda (x) x) '())", "#<void>");
    check_equal("(for-each (lambda (x) x) '(1))", "#<void>");
    check_equal("(for-each (lambda (x) x) '(1 2 3))", "#<void>");
    check_equal("(for-each (lambda (x y) x) '(1 2 3) '(a b c))", "#<void>");
    check_equal("(for-each (lambda (x y) (+ x y)) '(1 2 3) '(2 4 6))", "#<void>");

    check_equal("(map (lambda (x) x) '())", "'()");
    check_equal("(map (lambda (x) x) '(1))", "'(1)");
    check_equal("(map (lambda (x) x) '(1 2 3))", "'(1 2 3)");
    check_equal("(map (lambda (x y) x) '(1 2 3) '(a b c))", "'(1 2 3)");
    check_equal("(map (lambda (x y) (+ x y)) '(1 2 3) '(2 4 6))", "'(3 6 9)");

    check_equal("(andmap (lambda (x) (eq? x 'a)) '())", "#t");
    check_equal("(andmap (lambda (x) (eq? x 'a)) '(a))", "#t");
    check_equal("(andmap (lambda (x) (eq? x 'a)) '(b))", "#f");
    check_equal("(andmap (lambda (x) (eq? x 'a)) '(a a a))", "#t");
    check_equal("(andmap (lambda (x) (eq? x 'a)) '(a b a))", "#f");
    check_equal("(andmap (lambda (x) (eq? x 'a)) '(b b b))", "#f");

    check_equal("(ormap (lambda (x) (eq? x 'a)) '())", "#f");
    check_equal("(ormap (lambda (x) (eq? x 'a)) '(a))", "#t");
    check_equal("(ormap (lambda (x) (eq? x 'a)) '(b))", "#f");
    check_equal("(ormap (lambda (x) (eq? x 'a)) '(a a a))", "#t");
    check_equal("(ormap (lambda (x) (eq? x 'a)) '(a b a))", "#t");
    check_equal("(ormap (lambda (x) (eq? x 'a)) '(b b b))", "#f");

    return passed;
}

int test_integer() {
    passed = 1;

    check_equal("(+)", "0");
    check_equal("(+ 1)", "1");
    check_equal("(+ 1 2)", "3");
    check_equal("(+ 1 2 3)", "6");

    check_equal("(- 1)", "-1");
    check_equal("(- 1 2)", "-1");
    check_equal("(- 1 2 3)", "-4");

    check_equal("(*)", "1");
    check_equal("(* 1)", "1");
    check_equal("(* 1 2)", "2");
    check_equal("(* 1 2 3)", "6");

    check_equal("(/ 1 1)", "1");
    check_equal("(/ 6 3)", "2");
    check_equal("(/ 7 3)", "2");

    check_equal("(remainder 3 2)",    "1");
    check_equal("(remainder -3 2)",  "-1");
    check_equal("(remainder 3 -2)",   "1");
    check_equal("(remainder -3 -2)", "-1");

    check_equal("(modulo 3 2)",    "1");
    check_equal("(modulo -3 2)",   "1");
    check_equal("(modulo 3 -2)",  "-1");
    check_equal("(modulo -3 -2)", "-1");

    check_true ("(= 1 1)");
    check_false("(= 1 2)");

    check_true ("(>= 2 1)");
    check_true ("(>= 1 1)");
    check_false("(>= 0 1)");

    check_false("(<= 2 1)");
    check_true ("(<= 1 1)");
    check_true ("(<= 0 1)");

    check_true ("(> 2 1)");
    check_false("(> 1 1)");
    check_false("(> 0 1)");

    check_false("(< 2 1)");
    check_false("(< 1 1)");
    check_true ("(< 0 1)");

    return passed;
}

int test_string() {
    passed = 1;

    check_equal("(make-string 0)", "\"\"");
    check_equal("(make-string 1)", "\"a\"");
    check_equal("(make-string 3)", "\"aaa\"");
    check_equal("(make-string 1 #\\b)", "\"b\"");
    check_equal("(make-string 3 #\\b)", "\"bbb\"");

    check_equal("(string)", "\"\"");
    check_equal("(string #\\a)", "\"a\"");
    check_equal("(string #\\a #\\b)", "\"ab\"");
    check_equal("(string #\\a #\\b #\\c)", "\"abc\"");

    check_equal("(string-length \"\")", "0");
    check_equal("(string-length \"a\")", "1");
    check_equal("(string-length \"abc\")", "3");

    check_equal("(string-ref \"a\" 0)", "#\\a");
    check_equal("(string-ref \"ab\" 1)", "#\\b");
    check_equal("(string-ref \"abc\" 2)", "#\\c");

    check_equal("(begin (define s \"a\") (string-set! s 0 #\\b) s)", "\"b\"");
    check_equal("(begin (define s \"ab\") (string-set! s 1 #\\c) s)", "\"ac\"");
    check_equal("(begin (define s \"abc\") (string-set! s 2 #\\d) s)", "\"abd\"");

    check_equal("(string-append)", "\"\"");
    check_equal("(string-append \"foo\")", "\"foo\"");
    check_equal("(string-append \"foo\" \"bar\")", "\"foobar\"");
    check_equal("(string-append \"foo\" \"bar\" \"baz\")", "\"foobarbaz\"");

    return passed;
}

int test_syntax() {
    passed = 1;

    check_equal("(quote-syntax a)", "#<syntax a>");
    check_equal("(quote-syntax 1)", "#<syntax 1>");
    check_equal("(quote-syntax (a b c))", "#<syntax (a b c)>");

    check_equal("(syntax-e (quote-syntax a))", "'a");
    check_equal("(syntax-e (quote-syntax 1))", "1");
    check_equal("(syntax-e (quote-syntax (a b c)))",
                "'(#<syntax a> #<syntax b> #<syntax c>)");

    check_equal("(syntax->list (quote-syntax ()))", "'()");
    check_equal("(syntax->list (quote-syntax (1)))", "'(#<syntax 1>)");
    check_equal("(syntax->list (quote-syntax (1 2)))", "'(#<syntax 1> #<syntax 2>)");
    check_equal("(syntax->list (quote-syntax (1 2 3)))", "'(#<syntax 1> #<syntax 2> #<syntax 3>)");
    check_equal("(syntax->list (quote-syntax (1 2 . 3)))", "#f");
    check_equal("(syntax->list (quote-syntax 1))", "#f");

    return passed;
}

int test_if() {
    passed = 1;

    check_equal("(if #t 1 0)", "1");
    check_equal("(if 1 1 0)", "1");
    check_equal("(if '() 1 0)", "1");
    check_equal("(if #f 1 0)", "0");

    return passed;
}

int test_define() {
    passed = 1;

    check_equal("(define-values () (values))", "#<void>");
    check_equal("(define-values (x) 1)", "#<void>");
    check_equal("(define-values (x) (values 1))", "#<void>");
    check_equal("(define-values (x y) (values 1 2))", "#<void>");
    check_equal("(define-values (x y z) (values 1 2 3))", "#<void>");

    check_equal("(define x 1)", "#<void>");
    check_equal("(define (foo) 1)", "#<void>");
    check_equal("(define (foo x) 1)", "#<void>");
    check_equal("(define (foo x y) 1)", "#<void>");

    return passed;
}

int test_begin() {
    passed = 1;

    check_equal("(begin)", "#<void>");
    check_equal("(begin 1)", "1");
    check_equal("(begin 1 2)", "2");
    check_equal("(begin 1 2 3)", "3");

    return passed;
}

int test_values() {
    passed = 1;

    check_equal("(call-with-values (lambda () (values)) +)", "0");
    check_equal("(call-with-values (lambda () (values 1)) +)", "1");
    check_equal("(call-with-values (lambda () (values 1 2)) +)", "3");
    check_equal("(call-with-values (lambda () (values 1 2 3)) +)", "6");
    check_equal("(call-with-values (lambda () (values 1 2 3 4)) +)", "10");

    return passed;
}

int test_cond() {
    passed = 1;

    check_equal("(cond)", "#<void>");
    check_equal("(cond [#t 1])", "1");
    check_equal("(cond [1 1])", "1");
    check_equal("(cond [#t 1] [#f 0])", "1");
    check_equal("(cond [#f 1] [#t 0])", "0");
    check_equal("(cond [#f 1] [#f 0])", "#<void>");
    check_equal("(cond [#f 1] [#f 2] [#t 3])", "3");
    check_equal("(cond [#f 1] [else 2])", "2");
    check_equal("(cond [#f 1] [#f 2] [else 3])", "3");

    return passed;
}

int test_let() {
    passed = 1;

    check_equal("(letrec-values () 1)", "1");
    check_equal("(letrec-values ([() (values)]) 1)", "1");
    check_equal("(letrec-values ([(x) 1]) x)", "1");
    check_equal("(letrec-values ([(x y) (values 1 2)]) (list x y))", "'(1 2)");
    check_equal("(letrec-values ([(x y z) (values 1 2 3)]) (list x y z))", "'(1 2 3)");
    check_equal("(letrec-values ([(x) 1] [(y) 2]) (list x y))", "'(1 2)");
    check_equal("(letrec-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (list x y z))", "'(1 2 3)");
    check_equal("(letrec-values ([(f g) (values (lambda () (g 1)) (lambda (x) 1))]) (f))", "1");

    check_equal("(letrec () 1)", "1");
    check_equal("(letrec ([x 1]) x)", "1");
    check_equal("(letrec ([x 1] [y 2]) x)", "1");
    check_equal("(letrec ([x 1] [y 2]) y)", "2");
    check_equal("(letrec ([x 1] [y 2]) y x)", "1");
    check_equal("(letrec ([x 1]) (let ([y x]) y))", "1");
    check_equal("(letrec ([f (lambda () 1)]) (f))", "1");

    check_equal("(let-values () 1)", "1");
    check_equal("(let-values ([() (values)]) 1)", "1");
    check_equal("(let-values ([(x) 1]) x)", "1");
    check_equal("(let-values ([(x y) (values 1 2)]) (list x y))", "'(1 2)");
    check_equal("(let-values ([(x y z) (values 1 2 3)]) (list x y z))", "'(1 2 3)");
    check_equal("(let-values ([(x) 1] [(y) 2]) (list x y))", "'(1 2)");
    check_equal("(let-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (list x y z))", "'(1 2 3)");

    check_equal("(let () 1)", "1");
    check_equal("(let ([x 1]) x)", "1");
    check_equal("(let ([x 1] [y 2]) x)", "1");
    check_equal("(let ([x 1] [y 2]) y)", "2");
    check_equal("(let ([x 1] [y 2]) y x)", "1");
    check_equal("(let ([x 1]) (let ([y x]) y))", "1");

    return passed;
}

void run_tests() {
    log_test("simple eval", test_simple_eval);
    log_test("syntax", test_syntax);
    log_test("if", test_if);
    log_test("define", test_define);
    log_test("begin", test_begin);
    log_test("values", test_values);
    log_test("cond", test_cond);
    log_test("let", test_let);

    log_test("type predicates", test_type_predicates);
    log_test("eq?", test_eq);
    log_test("equal?", test_equal);
    log_test("type conversions", test_type_conversions);
    
    log_test("string", test_string);
    log_test("list", test_list);
    log_test("integer", test_integer);
}

int main(int argc, char **argv) {
    volatile int stack_top;

    GC_init(((void*) &stack_top));
    minim_boot_init();
    stream = tmpfile();

    return_code = 0;
    run_tests();

    fclose(stream);
    GC_finalize();
    return return_code;
}

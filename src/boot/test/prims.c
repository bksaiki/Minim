/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations

    Tests for the primitive library
*/

#include "../boot.h"

int return_code, passed;

#define load(stream, s) {   \
    fputs(s, stream);       \
    rewind(stream);         \
}

char *write(mobj o) {
    FILE *stream;
    char *buffer;
    size_t len, read;

    stream = tmpfile();
    write_object(stream, o);
    len = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    buffer = GC_alloc_atomic((len + 1) * sizeof(char));
    read = fread(buffer, 1, len, stream);
    buffer[len] = '\0';
    if (read != len) {
        fprintf(stderr, "read error occured");
        exit(1);
    }

    fclose(stream);
    return buffer;
}

#define log_failed_case(s, expect, actual) {                        \
    printf(" %s => expected: %s, actual: %s\n", s, expect, actual); \
}

void no_check(const char *input) {
    FILE *istream;

    istream = tmpfile();
    load(istream, input);
    eval_expr(read_object(istream), make_env());
}

void check_true(const char *input) {
    FILE *istream;
    mobj result;

    istream = tmpfile();
    load(istream, input);
    result = eval_expr(read_object(istream), make_env());
    if (!minim_truep(result)) {
        log_failed_case(input, "#t", write(result));
        passed = 0;
    }

    fclose(istream);
}

void check_false(const char *input) {
    FILE *istream;
    mobj result;

    istream = tmpfile();
    load(istream, input);
    result = eval_expr(read_object(istream), make_env());
    if (!minim_falsep(result)) {
        log_failed_case(input, "#f", write(result));
        passed = 0;
    }

    fclose(istream);
}

void check_equal(const char *input, const char *expect) {
    FILE *istream;
    mobj result;
    char *str;

    istream = tmpfile();
    load(istream, input);
    result = eval_expr(read_object(istream), make_env());
    str = write(result);
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
        passed = 0;
    }
    
    fclose(istream);
}

#define log_test(name, t) {             \
    if (t() == 1) {                     \
        printf("[ \033[32mPASS\033[0m ] %s\n", name);  \
    } else {                            \
        return_code = 1;                \
        printf("[ \033[31mFAIL\033[0m ] %s\n", name);  \
    }                                   \
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

    check_true ("(vector? #(1))");
    check_false("(vector? 1)");

    check_true ("(string? \"foo\")");
    check_false("(string? 1)");

    check_true ("(procedure? procedure?)");
    check_true ("(procedure? (lambda () 1))");
    check_false("(procedure? 1)");

    check_true ("(box? (box 3))");
    check_false("(box? 1)");

    check_true("(hashtable? (make-eq-hashtable))");
    check_false("(hashtable? 1)");

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

    check_true ("(eq? #() #())");
    check_false("(eq? #(1) #(1))");

    check_true ("(eq? car car)");
    check_false("(eq? car cdr)");

    check_true ("(let-values (((x) '(a))) (eq? x x))");
    check_true ("(let-values (((f) (lambda (x) x))) (eq? f f))");

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

    check_true("(equal? #() #())");
    check_true("(equal? #(1) #(1))");
    check_false("(equal? #(0) #(1))");
    check_true("(equal? #(1 2 3) #(1 2 3))");
    check_false("(equal? #(1 2) #(1 2 3))");

    check_true ("(let-values (((x) '(a))) (equal? x x))");
    check_true ("(let-values (((f) (lambda (x) x))) (equal? f f))");

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
    check_equal("(string->symbol \"a\")", "a");
    check_equal("(string->symbol \"foo\")", "foo");

    check_equal("(vector->list #())", "()");
    check_equal("(vector->list #(1))", "(1)");
    check_equal("(vector->list #(1 2 3))", "(1 2 3)");
    check_equal("(list->vector '())", "#()");
    check_equal("(list->vector '(1))", "#(1)");
    check_equal("(list->vector '(1 2 3))", "#(1 2 3)");

    return passed;
}

int test_list() {
    passed = 1;

    check_equal("(cons 1 2)", "(1 . 2)");
    check_equal("(cons 1 (cons 2 3))", "(1 2 . 3)");
    check_equal("(cons 1 '())", "(1)");

    check_equal("(car '(1 . 2))", "1");
    check_equal("(cdr '(1 . 2))", "2");
    check_equal("(car '(1 . (2 . 3)))", "1");
    check_equal("(cdr '(1 . (2 . 3)))", "(2 . 3)");
    check_equal("(car '(1))", "1");
    check_equal("(cdr '(1))", "()");

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

    check_equal("(list)", "()");
    check_equal("(list 1)", "(1)");
    check_equal("(list 1 2)", "(1 2)");
    check_equal("(list 1 2 3)", "(1 2 3)");

    check_equal("(length '())", "0");
    check_equal("(length '(1))", "1");
    check_equal("(length '(1 2))", "2");
    check_equal("(length '(1 2 3))", "3");

    check_equal("(reverse '())", "()");
    check_equal("(reverse '(1))", "(1)");
    check_equal("(reverse '(1 2))", "(2 1)");
    check_equal("(reverse '(1 2 3))", "(3 2 1)");

    check_equal("(append)", "()");
    check_equal("(append '())", "()");
    check_equal("(append '() '() '())", "()");
    check_equal("(append '(a))", "(a)");
    check_equal("(append '(a b c))", "(a b c)");
    check_equal("(append '(a b c) '(d))", "(a b c d)");
    check_equal("(append '(a b c) '(d e f))", "(a b c d e f)");

    check_equal("(for-each (lambda (x) x) '())", "#<void>");
    check_equal("(for-each (lambda (x) x) '(1))", "#<void>");
    check_equal("(for-each (lambda (x) x) '(1 2 3))", "#<void>");
    check_equal("(for-each (lambda (x y) x) '(1 2 3) '(a b c))", "#<void>");
    check_equal("(for-each (lambda (x y) (+ x y)) '(1 2 3) '(2 4 6))", "#<void>");
    check_equal("(begin (define-values (y) 0) (for-each (lambda (x) (set! y (+ y x))) '(1 2 3)) y)", "6");

    check_equal("(map (lambda (x) x) '())", "()");
    check_equal("(map (lambda (x) x) '(1))", "(1)");
    check_equal("(map (lambda (x) x) '(1 2 3))", "(1 2 3)");
    check_equal("(map (lambda (x y) x) '(1 2 3) '(a b c))", "(1 2 3)");
    check_equal("(map (lambda (x y) (+ x y)) '(1 2 3) '(2 4 6))", "(3 6 9)");

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

int test_vector() {
    passed = 1;

    check_equal("(make-vector 0)", "#()");
    check_equal("(make-vector 1)", "#(0)");
    check_equal("(make-vector 3)", "#(0 0 0)");

    check_equal("(make-vector 0 1)", "#()");
    check_equal("(make-vector 1 1)", "#(1)");
    check_equal("(make-vector 3 1)", "#(1 1 1)");

    check_equal("(vector)", "#()");
    check_equal("(vector 1)", "#(1)");
    check_equal("(vector 1 2 3)", "#(1 2 3)");

    check_equal("(vector-length #())", "0");
    check_equal("(vector-length #(1))", "1");
    check_equal("(vector-length #(1 2 3))", "3");

    check_equal("(vector-ref #(1 2 3) 0)", "1");
    check_equal("(vector-ref #(1 2 3) 1)", "2");
    check_equal("(vector-ref #(1 2 3) 2)", "3");

    check_equal("(begin (define-values (v) #(1 2 3)) (vector-set! v 0 0) v)", "#(0 2 3)");
    check_equal("(begin (define-values (v) #(1 2 3)) (vector-set! v 1 0) v)", "#(1 0 3)");
    check_equal("(begin (define-values (v) #(1 2 3)) (vector-set! v 2 0) v)", "#(1 2 0)");

    check_equal("(begin (define-values (v) #()) (vector-fill! v 0) v)", "#()");
    check_equal("(begin (define-values (v) #(1)) (vector-fill! v 0) v)", "#(0)");
    check_equal("(begin (define-values (v) #(1 2 3)) (vector-fill! v 0) v)", "#(0 0 0)");

    return passed;
}

int test_integer() {
    passed = 1;

    check_equal("(+)", "0");
    check_equal("(+ 1)", "1");
    check_equal("(+ 1 2)", "3");
    check_equal("(+ 1 2 3)", "6");
    check_equal("(add1 1)", "2");

    check_equal("(- 1)", "-1");
    check_equal("(- 1 2)", "-1");
    check_equal("(- 1 2 3)", "-4");
    check_equal("(sub1 1)", "0");

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

    check_equal("(begin (define-values (s) \"a\") (string-set! s 0 #\\b) s)", "\"b\"");
    check_equal("(begin (define-values (s) \"ab\") (string-set! s 1 #\\c) s)", "\"ac\"");
    check_equal("(begin (define-values (s) \"abc\") (string-set! s 2 #\\d) s)", "\"abd\"");

    check_equal("(string-append)", "\"\"");
    check_equal("(string-append \"foo\")", "\"foo\"");
    check_equal("(string-append \"foo\" \"bar\")", "\"foobar\"");
    check_equal("(string-append \"foo\" \"bar\" \"baz\")", "\"foobarbaz\"");

    check_equal("(format \"abc\")", "\"abc\"");
    check_equal("(format \"~a\" 1)", "\"1\"");
    check_equal("(format \"~a~a\" 1 2)", "\"12\"");
    check_equal("(format \"~a~a~a\" 1 2 3)", "\"123\"");

    return passed;
}

int test_syntax() {
    passed = 1;

    check_equal("(quote-syntax a)", "#<syntax>");
    check_equal("(quote-syntax 1)", "#<syntax>");
    check_equal("(quote-syntax (a b c))", "#<syntax>");

    check_equal("(syntax-e (quote-syntax a))", "a");
    check_equal("(syntax-e (quote-syntax 1))", "1");
    check_equal("(syntax-e (quote-syntax (a b c)))",
                "(#<syntax> #<syntax> #<syntax>)");

    check_equal("(syntax->list (quote-syntax ()))", "()");
    check_equal("(syntax->list (quote-syntax (1)))", "(#<syntax>)");
    check_equal("(syntax->list (quote-syntax (1 2)))", "(#<syntax> #<syntax>)");
    check_equal("(syntax->list (quote-syntax (1 2 3)))", "(#<syntax> #<syntax> #<syntax>)");
    check_equal("(syntax->list (quote-syntax (1 2 . 3)))", "#f");
    check_equal("(syntax->list (quote-syntax 1))", "#f");

    return passed;
}

int test_apply() {
    passed = 1;

    check_equal("(apply + '())", "0");
    check_equal("(apply + 1 '())", "1");
    check_equal("(apply + 1 2 3 '())", "6");
    check_equal("(apply + 1 '(2 3))", "6");
    check_equal("(apply + '(1 2 3))", "6");

    check_equal("(apply apply + '(()))", "0");
    check_equal("(apply apply + '(1 2 (3 4)))", "10");
    
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

int test_let() {
    passed = 1;

    check_equal("(letrec-values () 1)", "1");
    check_equal("(letrec-values ([() (values)]) 1)", "1");
    check_equal("(letrec-values ([(x) 1]) x)", "1");
    check_equal("(letrec-values ([(x y) (values 1 2)]) (list x y))", "(1 2)");
    check_equal("(letrec-values ([(x y z) (values 1 2 3)]) (list x y z))", "(1 2 3)");
    check_equal("(letrec-values ([(x) 1] [(y) 2]) (list x y))", "(1 2)");
    check_equal("(letrec-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (list x y z))", "(1 2 3)");
    check_equal("(letrec-values ([(f g) (values (lambda () (g 1)) (lambda (x) 1))]) (f))", "1");

    check_equal("(let-values () 1)", "1");
    check_equal("(let-values ([() (values)]) 1)", "1");
    check_equal("(let-values ([(x) 1]) x)", "1");
    check_equal("(let-values ([(x y) (values 1 2)]) (list x y))", "(1 2)");
    check_equal("(let-values ([(x y z) (values 1 2 3)]) (list x y z))", "(1 2 3)");
    check_equal("(let-values ([(x) 1] [(y) 2]) (list x y))", "(1 2)");
    check_equal("(let-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (list x y z))", "(1 2 3)");

    return passed;
}

int test_box() {
    passed = 1;

    check_equal("#&a)", "#&a");
    check_equal("#&(1 2 3))", "#&(1 2 3)");
    check_equal("#&#&3", "#&#&3");

    check_equal("(box 'a)", "#&a");
    check_equal("(box '(1 2 3))", "#&(1 2 3)");
    check_equal("(box (box 3))", "#&#&3");

    check_equal("(unbox (box 'a))", "a");
    check_equal("(unbox (box '(1 2 3)))", "(1 2 3)");
    check_equal("(unbox (box (box 3)))", "#&3");

    check_equal("(begin "
                  "(define-values (b) (box 'a))"
                  "(set-box! b 'b)"
                  "(unbox b))",
                "b");
    check_equal("(begin "
                  "(define-values (b) (box '(1 2 3)))"
                  "(set-box! b '())"
                  "(unbox b))",
                "()");
    check_equal("(begin "
                  "(define-values (b) (box (box 3)))"
                  "(set-box! b 0)"
                  "(unbox b))",
                "0");

    return passed;
}

int test_hashtable() {
    passed = 1;

    check_equal("(begin "
                  "(define-values (h) (make-eq-hashtable)) "
                  "(hashtable-set! h 'a 1) "
                  "(hashtable-size h))",
                "1");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-size h))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3)"
                   "(hashtable-size h))",
                "3");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-set! h 'd 4) "
                   "(hashtable-set! h 'e 5) "
                   "(hashtable-set! h 'f 6) "
                   "(hashtable-set! h 'g 7) "
                   "(hashtable-set! h 'h 8) "
                   "(hashtable-set! h 'i 9) "
                   "(hashtable-set! h 'j 10) "
                   "(hashtable-set! h 'k 11) "
                   "(hashtable-set! h 'l 12) "
                   "(hashtable-size h))",
                "12");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3)"
                   "(hashtable-set! h 'a 4) "
                   "(hashtable-set! h 'b 5) "
                   "(hashtable-set! h 'c 6)"
                   "(hashtable-size h))",
                "3");

    check_equal("(begin "
                  "(define-values (h) (make-hashtable)) "
                  "(hashtable-set! h 'a 1) "
                  "(hashtable-size h))",
                "1");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-size h))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3)"
                   "(hashtable-size h))",
                "3");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-set! h 'd 4) "
                   "(hashtable-set! h 'e 5) "
                   "(hashtable-set! h 'f 6) "
                   "(hashtable-set! h 'g 7) "
                   "(hashtable-set! h 'h 8) "
                   "(hashtable-set! h 'i 9) "
                   "(hashtable-set! h 'j 10) "
                   "(hashtable-set! h 'k 11) "
                   "(hashtable-set! h 'l 12) "
                   "(hashtable-size h))",
                "12");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3)"
                   "(hashtable-set! h 'a 4) "
                   "(hashtable-set! h 'b 5) "
                   "(hashtable-set! h 'c 6)"
                   "(hashtable-size h))",
                "3");

    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-ref h 'a))",
                "1");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-ref h 'b))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-ref h 'c))",
                "3");

    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h '() 1) "
                   "(hashtable-set! h '() 2) "
                   "(hashtable-ref h '()))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h #() 1) "
                   "(hashtable-set! h #() 2) "
                   "(hashtable-ref h #()))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h '(1) 1) "
                   "(hashtable-set! h '(1) 2) "
                   "(hashtable-ref h '(1)))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h #(1) 1) "
                   "(hashtable-set! h #(1) 2) "
                   "(hashtable-ref h #(1)))",
                "2");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-contains? h 'a))",
                "#t");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-contains? h 'b))",
                "#f");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-delete! h 'a) "
                   "(hashtable-contains? h 'a))",
                "#f");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(letrec-values ([(loop) "
                     "(lambda (i)"
                       "(if (= i 10000) "
                           "(void) "
                           "(begin "
                             "(hashtable-set! h i (+ i 1)) "
                             "(loop (+ i 1)))))]) "
                      "(loop 0)) "
                   "(hashtable-size h))",
                "10000");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-ref h 'a 0))",
                "0");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-ref h 'a (lambda () 0)))",
                "0");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-update! h 'a (lambda (x) (* 2 x))) "
                   "(hashtable-ref h 'a))",
                "2");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-update! h 'a (lambda (x) (* 2 x)) 2) "
                   "(hashtable-ref h 'a))",
                "4");
    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-update! h 'a (lambda (x) (* 2 x)) (lambda () 2)) "
                   "(hashtable-ref h 'a))",
                "4");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-ref (hashtable-copy h) 'a))",
                "1");

    check_equal("(begin "
                   "(define-values (h) (make-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-ref (hashtable-copy h) 'b))",
                "2");

    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-set! h 'd 4) "
                   "(hashtable-set! h 'e 5) "
                   "(hashtable-set! h 'f 6) "
                   "(hashtable-set! h 'g 7) "
                   "(hashtable-set! h 'h 8) "
                   "(hashtable-set! h 'i 9) "
                   "(hashtable-set! h 'j 10) "
                   "(hashtable-set! h 'k 11) "
                   "(hashtable-set! h 'l 12) "
                   "(hashtable-ref h 'a))",
                "1");
    check_equal("(begin "
                   "(define-values (h) (make-eq-hashtable)) "
                   "(hashtable-set! h 'a 1) "
                   "(hashtable-set! h 'b 2) "
                   "(hashtable-set! h 'c 3) "
                   "(hashtable-set! h 'd 4) "
                   "(hashtable-set! h 'e 5) "
                   "(hashtable-set! h 'f 6) "
                   "(hashtable-set! h 'g 7) "
                   "(hashtable-set! h 'h 8) "
                   "(hashtable-set! h 'i 9) "
                   "(hashtable-set! h 'j 10) "
                   "(hashtable-set! h 'k 11) "
                   "(hashtable-set! h 'l 12) "
                   "(hashtable-ref h 'k))",
                "11");

    return passed;
}

int test_port() {
    passed = 1;

    check_equal("(input-port? (current-input-port))", "#t");
    check_equal("(output-port? (current-input-port))", "#f");
    check_equal("(input-port? (current-output-port))", "#f");
    check_equal("(output-port? (current-output-port))", "#t");

    check_equal("(string-port? (current-input-port))", "#f");
    check_equal("(string-port? (current-output-port))", "#f");
    check_equal("(input-port? (open-input-string \"hello\"))", "#t");
    check_equal("(output-port? (open-input-string \"hello\"))", "#f");
    check_equal("(string-port? (open-input-string \"hello\"))", "#t");
    check_equal("(input-port? (open-output-string))", "#f");
    check_equal("(output-port? (open-output-string))", "#t");
    check_equal("(string-port? (open-output-string))", "#t");

    check_equal("(begin "
                  "(define-values (p) (open-input-string \"1\")) "
                  "(read p))",
                "1");

    check_equal("(begin "
                  "(define-values (p) (open-input-string \"(1 2 3)\")) "
                  "(read p))",
                "(1 2 3)");
    
    check_equal("(begin "
                  "(define-values (p) (open-output-string)) "
                  "(put-string p \"hello\") "
                  "(get-output-string p)) ",
                "\"hello\"");
    check_equal("(begin "
                  "(define-values (p) (open-output-string)) "
                  "(put-string p \"hello\") "
                  "(get-output-string p) "
                  "(get-output-string p))",
                "\"\"");
    check_equal("(begin "
                  "(define-values (p) (open-output-string)) "
                  "(put-string p \"hello\") "
                  "(get-output-string p) "
                  "(put-string p \"bye\")"
                  "(get-output-string p))",
                "\"bye\"");

    return passed;
}

void run_tests() {
    log_test("syntax", test_syntax);
    log_test("if", test_if);
    log_test("define", test_define);
    log_test("begin", test_begin);
    log_test("values", test_values);
    log_test("let", test_let);

    log_test("type predicates", test_type_predicates);
    log_test("eq?", test_eq);
    log_test("equal?", test_equal);
    log_test("type conversions", test_type_conversions);
    
    log_test("string", test_string);
    log_test("list", test_list);
    log_test("vector", test_vector);
    log_test("integer", test_integer);
    log_test("box", test_box);
    log_test("hashtable", test_hashtable);
    log_test("port", test_port);

    log_test("apply", test_apply);
}

int main(int argc, char **argv) {
    volatile int stack_top;

    GC_init(((void*) &stack_top));
    minim_boot_init();
    load_prelude(global_env(current_thread()));

    stack_top = 0;
    return_code = stack_top;
    run_tests();

    GC_finalize();
    return return_code;
}

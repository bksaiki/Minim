// prims.c: tests for kernel.min

#include "test.h"

int return_code, passed;

char *write_debug(obj o) {
    FILE *stream;
    char *buffer;
    size_t len, read;

    stream = tmpfile();
    write_obj(stream, o);
    len = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    buffer = GC_malloc_atomic((len + 1) * sizeof(char));
    read = fread(buffer, 1, len, stream);
    buffer[len] = '\0';
    if (read != len) {
        fprintf(stderr, "read error occured");
        exit(1);
    }

    fclose(stream);
    return buffer;
}

void check_equal(const char *input, const char *expect) {
    obj p, e, tc, x;
    char *str;

    p = Minput_string_port(Mstring(input));
    e = read_object(p);

    tc = Mcurr_tc();
    Mtc_env(tc) = empty_env();
    env_import(Mtc_env(tc), Mmodule_env(Mkernel));
    x = eval_expr(e);

    str = write_debug(x);
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
        passed = 0;
    }
}

#define check_true(input) \
    check_equal(input, "#t")

#define check_false(input) \
    check_equal(input, "#f")

int test_type(void) {
    passed = 1;

    check_true ("(boolean? #t)");
    check_true ("(boolean? #f)");
    check_false("(boolean? 'a)");

    check_true ("(symbol? 'a)");
    check_false("(symbol? 1)");

    check_true ("(fixnum? 1)");
    check_false("(fixnum? 'a)");

    // check_true ("(char? #\\a)");
    // check_false("(char? 1)");

    check_true ("(vector? #(1))");
    check_false("(vector? 1)");

    check_true ("(string? \"foo\")");
    check_false("(string? 1)");

    check_true ("(procedure? procedure?)");
    check_true ("(procedure? (lambda () 1))");
    check_false("(procedure? 1)");

    return passed;
}

int test_eq(void) {
    passed = 1;

    check_true ("(eq? #t #t)");
    check_true ("(eq? #f #f)");
    check_false("(eq? #t #f)");

    check_true ("(eq? 1 1)");
    check_false("(eq? 1 2)");

    // check_true ("(eq? #\\a #\\a)");
    // check_false("(eq? #\\a #\\b)");

    check_false("(eq? \"\" \"\")");
    check_false("(eq? \"a\" \"a\")");

    check_true ("(eq? '() '())");
    check_false("(eq? '(1) '(1))");
    check_false("(eq? '(1 . 2) '(1 . 2))");

    check_true ("(eq? #() #())");
    check_false("(eq? #(1) #(1))");
    check_false("(eq? #(1 2 3) #(1 2 3))");

    check_true ("(eq? car car)");
    check_false("(eq? car cdr)");

    check_true ("(let ([x '(a)]) (eq? x x))");
    check_true ("(let ([f (lambda (x) x)]) (eq? f f))");

    return passed;
}

int test_equal(void) {
    passed = 1;

    check_true ("(equal? #t #t)");
    check_true ("(equal? #f #f)");
    check_false("(equal? #t #f)");

    check_true ("(equal? 1 1)");
    check_false("(equal? 1 2)");

    // check_true ("(equal? #\\a #\\a)");
    // check_false("(equal? #\\a #\\b)");

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

    check_true ("(let ([x '(a)]) (equal? x x))");
    check_true ("(let ([f (lambda (x) x)]) (equal? f f))");

    return passed;
}

int test_num(void) {
    passed = 1;

    check_equal("(+)", "0");
    check_equal("(+ 1)", "1");
    check_equal("(+ 1 2)", "3");
    check_equal("(+ 1 2 3)", "6");
    check_equal("(1+ 1)", "2");

    check_equal("(- 1)", "-1");
    check_equal("(- 1 2)", "-1");
    check_equal("(- 1 2 3)", "-4");
    check_equal("(1- 1)", "0");

    check_equal("(*)", "1");
    check_equal("(* 1)", "1");
    check_equal("(* 1 2)", "2");
    check_equal("(* 1 2 3)", "6");

    check_equal("(/ 1 1)", "1");
    check_equal("(/ 6 3)", "2");
    check_equal("(/ 7 3)", "2");
    check_equal("(/ 24 2 3)", "4");

    check_equal("(remainder 3 2)", "1");
    check_equal("(remainder -3 2)", "-1");
    check_equal("(remainder 3 -2)", "1");
    check_equal("(remainder -3 -2)", "-1");

    check_equal("(modulo 3 2)", "1");
    check_equal("(modulo -3 2)", "1");
    check_equal("(modulo 3 -2)", "-1");
    check_equal("(modulo -3 -2)", "-1");

    check_true("(= 1)");
    check_true ("(= 1 1)");
    check_false("(= 1 2)");
    check_true("(= 1 1 1)");
    check_false("(= 1 2 1)");
    check_false("(= 1 1 2)");

    check_true("(>= 1)");
    check_true ("(>= 2 1)");
    check_true ("(>= 1 1)");
    check_false("(>= 0 1)");
    check_true("(>= 2 1 1)");
    check_false("(>= 1 2 1)");
    check_false("(>= 1 1 2)");

    check_true("(<= 1)");
    check_false("(<= 2 1)");
    check_true ("(<= 1 1)");
    check_true ("(<= 0 1)");
    check_true("(<= 1 1 2)");
    check_false("(<= 1 2 1)");
    check_false("(<= 2 1 1)");

    check_true("(> 1)");
    check_true ("(> 2 1)");
    check_false("(> 1 1)");
    check_false("(> 0 1)");

    check_true("(< 1)");
    check_false("(< 2 1)");
    check_false("(< 1 1)");
    check_true ("(< 0 1)");

    return passed;
}

int test_pair(void) {
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

    return passed;
}

int test_list(void) {
    passed = 1;

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
    check_equal("(let ([y 0]) (for-each (lambda (x) (set! y (+ y x))) '(1 2 3)) y)", "6");

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

int test_vector(void) {
    passed = 1;

    check_equal("(vector)", "#()");
    check_equal("(vector 1)", "#(1)");
    check_equal("(vector 1 2 3)", "#(1 2 3)");

    check_equal("(make-vector 0)", "#()");
    check_equal("(make-vector 1)", "#(0)");
    check_equal("(make-vector 3)", "#(0 0 0)");

    check_equal("(make-vector 0 1)", "#()");
    check_equal("(make-vector 1 1)", "#(1)");
    check_equal("(make-vector 3 1)", "#(1 1 1)");

    check_equal("(vector-length #())", "0");
    check_equal("(vector-length #(1))", "1");
    check_equal("(vector-length #(1 2 3))", "3");

    check_equal("(vector-ref #(1 2 3) 0)", "1");
    check_equal("(vector-ref #(1 2 3) 1)", "2");
    check_equal("(vector-ref #(1 2 3) 2)", "3");

    check_equal("(let-values ([(v) #(1 2 3)]) (vector-set! v 0 0) v)", "#(0 2 3)");
    check_equal("(let-values ([(v) #(1 2 3)]) (vector-set! v 1 0) v)", "#(1 0 3)");
    check_equal("(let-values ([(v) #(1 2 3)]) (vector-set! v 2 0) v)", "#(1 2 0)");

    check_equal("(vector->list #())", "()");
    check_equal("(vector->list #(1))", "(1)");
    check_equal("(vector->list #(1 2 3))", "(1 2 3)");

    check_equal("(list->vector '())", "#()");
    check_equal("(list->vector '(1))", "#(1)");
    check_equal("(list->vector '(1 2 3))", "#(1 2 3)");


    return passed;
}

int test_callwv(void) {
    passed = 1;

    check_equal("(call-with-values (lambda () (values)) (lambda xs xs))", "()");
    check_equal("(call-with-values (lambda () (values 1)) (lambda xs xs))", "(1)");
    check_equal("(call-with-values (lambda () (values 1 2 3)) (lambda xs xs))", "(1 2 3)");
    check_equal("(call-with-values (lambda () (values 1 2)) +)", "3");

    return passed;
}

int test_callcc(void) {
    passed = 1;

    check_equal("(call/cc (lambda (k) 1))", "1");
    check_equal("(call/cc (lambda (k) (k 1) 2))", "1");
    check_equal("(let ([x #f]) (cons 1 (call/cc (lambda (k) (set! x k) 2))))", "(1 . 2)");

    // from ChezScheme documentation
    check_equal("(call/cc (lambda (k) (* 5 (k 4))))", "4");
    check_equal("(+ 2 (call/cc (lambda (k) (* 5 (k 4)))))", "6");
    check_equal("(letrec ([product "
                  "(lambda (xs) "
                    "(call/cc "
                      "(lambda (break) "
                        "(if (null? xs) "
                            "1 "
                            "(if (= (car xs) 0) "
                                "(break 0) "
                                "(* (car xs) (product (cdr xs))))))))]) "
                  "(product '(7 3 8 0 1 9 5)))",
                "0");
    check_equal("(let ([x (call/cc (lambda (k) k))]) "
                  "(x (lambda (ignore) \"hi\")))",
                "\"hi\"");
    
    check_equal("(letrec ([k* #f] "
                         "[y (1+ (call/cc (lambda (k) (set! k* k) 0)))]) "
                  "(if (< y 5) "
                      "(k* y) "
                      "y))",
                "5");

    return passed;
}

int test_dynamic_wind(void) {
    passed = 1;

    check_equal("(dynamic-wind (lambda () 1) (lambda () 2) (lambda () 3))", "2");
    check_equal("(dynamic-wind (lambda () (values)) (lambda () 1) (lambda () (values 1 2)))", "1");

    check_equal(
        "(let ((path '()) (c #f)) "
          "(let ((add (lambda (s) "
                       "(set! path (cons s path))))) "
            "(dynamic-wind "
              "(lambda () (add 'connect)) "
              "(lambda () "
                "(add (call/cc "
                        "(lambda (c0) "
                          "(set! c c0) "
                          "'talk1)))) "
              "(lambda () (add 'disconnect))) "
           "(if (< (length path) 4) "
               "(c 'talk2) "
               "(reverse path))))",
        "(connect talk1 disconnect connect talk2 disconnect)"
    );

    return passed;
}

int test_apply(void) {
    passed = 1;

    check_equal("(apply list '())", "()");
    check_equal("(apply list 1 '())", "(1)");
    check_equal("(apply list 1 '(2 3))", "(1 2 3)");
    check_equal("(apply list 1 2 3 '())", "(1 2 3)");
    check_equal("(apply list 1 2 3 '(4 5))", "(1 2 3 4 5)");

    check_equal(
        "(apply call-with-values "
          "(cons (lambda () 1) "
          "(cons (lambda (x) (cons x 2)) "
          "'())))",
        "(1 . 2)"
    );

    return passed;
}

int test_misc(void) {
    passed = 1;

    check_equal("(void)", "#<void>");
    check_equal("(void 1)", "#<void>");
    check_equal("(void 1 2 3)", "#<void>");

    return passed;
}

int main(int argc, char **argv) {
    GC_init();
    minim_init();
    load_kernel();  

    log_test("type", test_type, return_code);
    log_test("eq", test_eq, return_code);
    log_test("equal", test_equal, return_code);
    log_test("num", test_num, return_code);
    log_test("pair", test_pair, return_code);
    log_test("list", test_list, return_code);
    log_test("vector", test_vector, return_code);
    log_test("call-with-values", test_callwv, return_code);
    log_test("call/cc", test_callcc, return_code);
    log_test("dynamic-wind", test_dynamic_wind, return_code);
    log_test("apply", test_apply, return_code);
    log_test("misc", test_misc, return_code);

    return_code = 0;

    minim_shutdown(return_code);
}

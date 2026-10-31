// syntax.c: tests for evaluation

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
    Mtc_env(tc) = prim_env(empty_env());
    x = eval_expr(e);

    str = write_debug(x);
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
        passed = 0;
    }
}

int test_quote(void) {
    passed = 1;

    check_equal("'1", "1");
    check_equal("'a", "a");
    check_equal("'foo", "foo");
    check_equal("'()", "()");
    check_equal("'(1)", "(1)");
    check_equal("'(1 2 3)", "(1 2 3)");

    return passed;
}

int test_if(void) {
    passed = 1;

    check_equal("(if #f 1 0)", "0");
    check_equal("(if #t 1 0)", "1");
    check_equal("(if 'a 1 0)", "1");
    check_equal("(if 1 1 0)", "1");

    return passed;
}

int test_when(void) {
    passed = 1;

    check_equal("(when #t 1)", "1");
    check_equal("(when #f 1)", "#<void>");

    return passed;
}

int test_unless(void) {
    passed = 1;

    check_equal("(unless #t 1)", "#<void>");
    check_equal("(unless #f 1)", "1");

    return passed;
}

int test_cond(void) {
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

int test_begin(void) {
    passed = 1;

    check_equal("(begin)", "#<void>");
    check_equal("(begin 1)", "1");
    check_equal("(begin 1 2 3)", "3");
    check_equal("(begin (begin) (begin 1 2) 3)", "3");

    return passed;
}

int test_let_values(void) {
    passed = 1;

    check_equal("(let-values () 1)", "1");
    check_equal("(let-values ([() (values)]) 1)", "1");
    check_equal("(let-values ([(x) 1]) x)", "1");
    check_equal("(let-values ([(x y) (values 1 2)]) (cons x y))", "(1 . 2)");
    check_equal("(let-values ([(x y z) (values 1 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");
    check_equal("(let-values ([(x) 1] [(y) 2]) (cons x y))", "(1 . 2)");
    check_equal("(let-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");

    return passed;
}

int test_letrec_values(void) {
    passed = 1;

    check_equal("(letrec-values () 1)", "1");
    check_equal("(letrec-values ([() (values)]) 1)", "1");
    check_equal("(letrec-values ([(x) 1]) x)", "1");
    check_equal("(letrec-values ([(x y) (values 1 2)]) (cons x y))", "(1 . 2)");
    check_equal("(letrec-values ([(x y z) (values 1 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");
    check_equal("(letrec-values ([(x) 1] [(y) 2]) (cons x y))", "(1 . 2)");
    check_equal("(letrec-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");
    check_equal("(letrec-values ([(f g) (values (lambda () (g 1)) (lambda (x) 1))]) (f))", "1");

    return passed;
}

int test_let(void) {
    passed = 1;

    check_equal("(let () 1)", "1");
    check_equal("(let ([x 1]) x)", "1");
    check_equal("(let ([x 1] [y 2]) y)", "2");
    check_equal("(let ([x 1] [y 2] [z 3]) (cons x (cons y z)))", "(1 2 . 3)");

    check_equal("(let ([x 1]) (cons (let ([x 2]) x) x))", "(2 . 1)");
    check_equal("(let ([x 1]) (begin (let ([x 2]) x) x))", "1");
    check_equal("(let ([x 1]) (if (let ([x 2]) #t) x 0))", "1");
    check_equal("(let ([x 1]) (if (let ([x 2]) #f) x 0))", "0");

    return passed;
}

int test_letrec(void) {
    passed = 1;

    check_equal("(letrec () 1)", "1");
    check_equal("(letrec ([x 1]) x)", "1");
    check_equal("(letrec ([x 1] [y 2]) y)", "2");
    check_equal("(letrec ([x 1] [y 2] [z 3]) (cons x (cons y z)))", "(1 2 . 3)");

    check_equal("(letrec ([x 1] [y x]) y)", "1");
    check_equal("(letrec ([f (lambda () f)]) (f))", "#<procedure:f>");
    check_equal("(letrec ([f (lambda () f)] [g (lambda () f)]) (g))", "#<procedure:f>");
    check_equal("(letrec ([f (lambda () g)] [g (lambda () f)]) (g))", "#<procedure:f>");

    return passed;
}

int test_let_loop(void) {
    passed = 1;

    check_equal("(let f () 1)", "1");
    check_equal("(let f ([x 5]) (if (fx<= x 0) #t (f (fx1- x))))", "#t");
    check_equal("(let f ([x 5] [y 3]) (if (fx<= x 0) y (f (fx1- x) (fx1+ y))))", "8");

    return passed;
}

int test_lambda(void) {
    passed = 1;

    check_equal("(lambda () 1)", "#<procedure>");
    check_equal("(lambda xs xs)", "#<procedure>");
    check_equal("(lambda (x) x)", "#<procedure>");
    check_equal("(lambda (x y z) (cons x z))", "#<procedure>");
    check_equal("(lambda (x y . zs) (cons x zs))", "#<procedure>");

    check_equal("((lambda () 1))", "1");
    check_equal("((lambda xs xs))", "()");
    check_equal("((lambda (x) x) 1)", "1");
    check_equal("((lambda (x y z) (cons x z)) 1 2 3)", "(1 . 3)");
    check_equal("((lambda (x y . zs) (cons x zs)) 1 2)", "(1)");
    check_equal("((lambda (x y . zs) (cons x zs)) 1 2 3)", "(1 3)");
    check_equal("((lambda (x y . zs) (cons x zs)) 1 2 3 4 5)", "(1 3 4 5)");


    return passed;
}

int test_case_lambda(void) {
    passed = 1;

    check_equal("((case-lambda [() 0]))", "0");
    check_equal("((case-lambda [(a) 1]))", "1");
    check_equal("((case-lambda [(a b) 2]))", "2");
    check_equal("((case-lambda [(a b c) 3]))", "3");

    return passed;
}

int test_setb(void) {
    passed = 1;

    check_equal("(let ([x 1]) (set! x 2) x)", "2");
    check_equal("(let ([x 1] [y 2]) (set! x 2) (set! y 3) (cons x y))", "(2 . 3)");
    check_equal("(let ([x 1]) ((lambda () (set! x 2))) x) ", "2");
    check_equal("(let ([x 1]) ((lambda (y) (set! x 2) (cons x y)) x)) ", "(2 . 1)");

    return passed;
}

int main(int argc, char **argv) {
    GC_init();
    minim_init();

    return_code = 0;

    log_test("quote", test_quote, return_code);
    log_test("if", test_if, return_code);
    log_test("when", test_when, return_code);
    log_test("unless", test_unless, return_code);
    log_test("cond", test_cond, return_code);
    log_test("begin", test_begin, return_code);
    log_test("let-values", test_let_values, return_code);
    log_test("letrec-values", test_letrec_values, return_code);
    log_test("let", test_let, return_code);
    log_test("letrec", test_letrec, return_code);
    log_test("let (loop)", test_let_loop, return_code);
    log_test("lambda", test_lambda, return_code);
    log_test("case-lambda", test_case_lambda, return_code);
    log_test("set!", test_setb, return_code);

    minim_shutdown(return_code);
}

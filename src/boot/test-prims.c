/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations

    Tests for the primitive library
*/

#include "boot.h"

FILE *stream;
int return_code, passed;

#define reset() {               \
    rewind(stream);             \
    global_env = make_env();    \
}

#define load(s) {       \
    fputs(s, stream);   \
    rewind(stream);     \
}

#define eval(s)     eval_expr(read_object(stream), global_env)

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

int test_lists() {
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

    check_equal("(list)", "'()");
    check_equal("(list 1)", "'(1)");
    check_equal("(list 1 2)", "'(1 2)");
    check_equal("(list 1 2 3)", "'(1 2 3)");

    return passed;
}

int test_integers() {
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

    return passed;
}

void run_tests() {
    log_test("simple eval", test_simple_eval);
    log_test("type predicates", test_type_predicates);
    log_test("eq?", test_eq);
    log_test("equal?", test_equal);
    log_test("type conversions", test_type_conversions);
    log_test("lists", test_lists);
    log_test("integers", test_integers);
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

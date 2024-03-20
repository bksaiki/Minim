// jit.c: tests for the JIT

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

int test_simple() {
    passed = 1;

    no_check("1");
    no_check("'a");
    no_check("#\\a");
    no_check("#()");
    no_check("#(1)");
    no_check("#(1 2 3)");
    no_check("\"foo\"");
    no_check("'()");
    no_check("'(1 . 2)");
    no_check("'(1 2 3)");
    no_check("'(1 2 . 3)");

    return passed;
}

int test_if() {
    passed = 1;

    check_equal("(if #t 1 0)", "1");
    check_equal("(if #f 1 0)", "0");
    check_equal("(if 1 1 0)", "1");
    check_equal("(if 0 1 0)", "1");

    return passed;
}

int test_closure() {
    passed = 1;

    check_equal("(lambda () 1)", "#<procedure>");
    check_equal("(lambda (x) (cons x 1))", "#<procedure>");
    check_equal("(lambda (x y) (cons x y))", "#<procedure>");
    check_equal("(lambda (x y . rest) ($append rest))", "#<procedure>");

    return passed;
}

int test_app() {
    passed = 1;

    check_equal("(void)", "#<void>");
    check_equal("(null? '())", "#t");
    check_equal("(cons 'a 'b)", "(a . b)");
    check_equal("((lambda () 1))", "1");
    check_equal("(cons (cons 'a 'b) (cons 'c 'd))", "((a . b) c . d)");
    check_equal("(cons ((lambda () 'a)) (cons 'b 'c))", "(a b . c)");
    check_equal("((lambda (x) (cons x 2)) 1)", "(1 . 2)");

    return passed;
}

int test_let_values() {
    passed = 1;

    check_equal("(letrec-values () 1)", "1");
    check_equal("(letrec-values ([() (values)]) 1)", "1");
    check_equal("(letrec-values ([(x) 1]) x)", "1");
    check_equal("(letrec-values ([(x y) (values 1 2)]) (cons x y))", "(1 . 2)");
    check_equal("(letrec-values ([(x y z) (values 1 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");
    check_equal("(letrec-values ([(x) 1] [(y) 2]) (cons x y))", "(1 . 2)");
    check_equal("(letrec-values ([() (values)] [(x) 1] [(y z) (values 2 3)]) (cons x (cons y z)))", "(1 2 . 3)");

    return passed;
}

int main(int argc, char **argv) {
    volatile int stack_top;

    GC_init(((void*) &stack_top));
    minim_boot_init();

    stack_top = 0;
    return_code = stack_top;

    log_test("simple", test_simple);
    log_test("if", test_if);
    log_test("closure", test_closure);
    log_test("app", test_app);
    log_test("let-values", test_let_values);

    GC_finalize();
    return return_code;
}


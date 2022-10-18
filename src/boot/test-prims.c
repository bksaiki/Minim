/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations

    Tests for the primitive library
*/

#include "boot.h"

FILE *stream;
minim_object *result;
int passed;

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

#define check_true(s) {                             \
    reset();                                        \
    load(s);                                        \
    result = eval();                                \
    if (!minim_is_true(result)) {                   \
        log_failed_case(s, "#t", write(result));    \
    }                                               \
}

#define check_false(s) {                            \
    reset();                                        \
    load(s);                                        \
    result = eval();                                \
    if (!minim_is_false(result)) {                  \
        log_failed_case(s, "#f", write(result));    \
    }                                               \
}

#define log_test(name, t) {             \
    if (t() == 1) {                     \
        printf("[PASSED] %s\n", name);  \
    } else {                            \
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

int test_type_conversions() {
    passed = 1;

    return passed;
}


int run_tests() {
    log_test("simple eval", test_simple_eval);
    log_test("type predicates", test_type_predicates);
    return 0;
}

int main(int argc, char **argv) {
    volatile int stack_top;
    int return_code;

    GC_init(((void*) &stack_top));
    minim_boot_init();
    stream = tmpfile();

    return_code = run_tests();

    fclose(stream);
    GC_finalize();
    return return_code;
}

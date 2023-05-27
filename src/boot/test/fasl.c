/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations

    Tests for FASL reading and writing
*/

#include "../boot.h"

FILE *stream, *fasl_stream;
minim_object *env;
int return_code, passed;

#define reset() {           \
    rewind(stream);         \
    env = make_env();       \
}

#define load(s) {       \
    fputs(s, stream);   \
    rewind(stream);     \
}

#define eval()      eval_expr(read_object(stream), env)

#define log_failed_case(s, expect, actual) {                            \
    printf(" %s => expected: %s, actual: %s\n", s, expect, actual);     \
}

void check_fasl_equal(const char *input, const char *expect) {
    minim_object *o;
    char *str;
    long len, read;

    stream = tmpfile();
    fasl_stream = tmpfile();

    // evaluate input string
    reset();
    load(input);
    o = eval();
    
    // write as FASL
    write_fasl(fasl_stream, o);
    rewind(fasl_stream);

    // read from FASL
    o = read_fasl(fasl_stream);

    // write object (should be the same)
    rewind(stream);
    write_object(stream, o);

    len = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    str = GC_alloc_atomic((len + 1) * sizeof(char));
    read = fread(str, 1, len, stream);
    str[len] = '\0';

    fclose(stream);
    fclose(fasl_stream);

    if (read != len) {
        fprintf(stderr, "read error occured");
        exit(1);
    }
    
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
    }
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

    check_fasl_equal("'()", "'()");
    check_fasl_equal("#t", "#t");
    check_fasl_equal("#f", "#f");
    check_fasl_equal("eof", "#<eof>");
    check_fasl_equal("(void)", "#<void>");

    check_fasl_equal("#\\a", "#\\a");
    check_fasl_equal("#\\b", "#\\b");
    check_fasl_equal("#\\0", "#\\0");

    check_fasl_equal("'a", "'a");
    check_fasl_equal("'abc", "'abc");
    check_fasl_equal("'abc123", "'abc123");

    check_fasl_equal("\"\"", "\"\"");
    check_fasl_equal("\"a\"", "\"a\"");
    check_fasl_equal("\"abc\"", "\"abc\"");
    check_fasl_equal("\"abc123\"", "\"abc123\"");

    check_fasl_equal("0", "0");
    check_fasl_equal("123", "123");
    check_fasl_equal("123456", "123456");

    return passed;
}

int test_pair() {
    passed = 1;

    check_fasl_equal("'(1 . 2)", "'(1 . 2)");
    check_fasl_equal("'(1 2 . 3)", "'(1 2 . 3)");
    check_fasl_equal("'(1 2 3 . 4)", "'(1 2 3 . 4)");

    check_fasl_equal("'(1)", "'(1)");
    check_fasl_equal("'(1 2)", "'(1 2)");
    check_fasl_equal("'(1 2 3)", "'(1 2 3)");

    check_fasl_equal("'((a . 1) (b . 2) (c . 3))", "'((a . 1) (b . 2) (c . 3))");

    return passed;
}

int test_vector() {
    passed = 1;

    check_fasl_equal("'#()", "'#()");
    check_fasl_equal("'#(1)", "'#(1)");
    check_fasl_equal("'#(1 2 3)", "'#(1 2 3)");

    check_fasl_equal("'#((a . 1) (b . 2) (c . 3))", "'#((a . 1) (b . 2) (c . 3))");

    return passed;
}

int test_hashtable() {
    passed = 1;

    check_fasl_equal("(make-hashtable)", "#<hashtable>");

    check_fasl_equal("(begin "
                       "(define ht (make-hashtable)) "
                       "(hashtable-set! ht 'a 1) "
                       "ht)",
                     "#<hashtable (a . 1)>");
    
    check_fasl_equal("(begin "
                       "(define ht (make-hashtable)) "
                       "(hashtable-set! ht 'a 1) "
                       "(hashtable-set! ht 'b 2) "
                       "(hashtable-set! ht 'c 3) "
                       "ht)",
                     "#<hashtable (a . 1) (c . 3) (b . 2)>");

    return passed;
}

int test_record() {
    passed = 1;

    check_fasl_equal("(make-record-type-descriptor 'foo #f #f #f #f #())", "#<record-type:foo>");
    check_fasl_equal("($make-record "
                      "(make-record-type-descriptor 'foo #f #f #f #f #()))",
                     "#<foo>");

    check_fasl_equal("(make-record-type-descriptor 'bar #f #f #f #f "
                      "#((immutable a)))",
                     "#<record-type:bar>");
    check_fasl_equal("($make-record "
                      "(make-record-type-descriptor 'bar #f #f #f #f "
                       "#((immutable a)))"
                      "1)",
                     "#<bar>");

    check_fasl_equal("(make-record-type-descriptor 'baz #f #f #f #f "
                      "#((immutable a) (immutable b) (immutable c)))",
                     "#<record-type:baz>");
    check_fasl_equal("($make-record "
                      "(make-record-type-descriptor 'baz #f #f #f #f "
                       "#((immutable a) (immutable b) (immutable c)))"
                      "1 2 3)",
                     "#<baz>");

    return passed;
}

void run_tests() {
    log_test("simple", test_simple);
    log_test("pair", test_pair);
    log_test("vector", test_vector);
    log_test("hashtable", test_hashtable);
    log_test("record", test_record);
}

int main(int argc, char **argv) {
    volatile int stack_top;

    GC_init(((void*) &stack_top));
    minim_boot_init();

    stack_top = 0;
    return_code = stack_top;
    run_tests();

    GC_finalize();
    return return_code;
}

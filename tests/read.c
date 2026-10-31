// read.c: tests for C implementation of a Scheme reader

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
    obj p, e;
    char *str;

    p = Minput_string_port(Mstring(input));
    e = read_object(p);

    str = write_debug(e);
    if (strcmp(str, expect) != 0) {
        log_failed_case(input, expect, str);
        passed = 0;
    }
}

int test_read_simple() {
    passed = 1;

    check_equal("#t", "#t");
    check_equal("#f", "#f");

    check_equal("0", "0");
    check_equal("1", "1");
    check_equal("123", "123");
    check_equal("12345", "12345");
    check_equal("+1", "1");
    check_equal("-1", "-1");
    check_equal("+123", "123");
    check_equal("-123", "-123");

    check_equal("#x0", "0");
    check_equal("#x1", "1");
    check_equal("#xa", "10");
    check_equal("#xA", "10");
    check_equal("#xfe", "254");

    check_equal("a", "a");
    check_equal("abc", "abc");
    check_equal("abcde", "abcde");

    check_equal("+", "+");
    check_equal("-", "-");
    check_equal("+a", "+a");
    check_equal("-a", "-a");
    check_equal("+1a", "+1a");
    check_equal("-1a", "-1a");
    
    check_equal("#\\a", "#\\a");
    check_equal("#\\space", "#\\space");
    check_equal("#\\0", "#\\0");

    return passed;
}

int test_read_pair() {
    passed = 1;

    check_equal("()", "()");
    check_equal("(1 . 2)", "(1 . 2)");
    check_equal("(1 . (2 . 3))", "(1 2 . 3)");
    check_equal("((1 . 2) . (3 . 4))", "((1 . 2) 3 . 4)");

    check_equal("(1)", "(1)");
    check_equal("(1 2)", "(1 2)");
    check_equal("(1 2 3)", "(1 2 3)");
    check_equal("(1 2 3 4)", "(1 2 3 4)");

    check_equal("((1 2) (3 4))", "((1 2) (3 4))");
    check_equal("((1 2) (3 4) (5 6))", "((1 2) (3 4) (5 6))");

    return passed;
}

int test_read_vector(void) {
    passed = 1;

    check_equal("#()", "#()");
    check_equal("#(1)", "#(1)");
    check_equal("#(1 2 3)", "#(1 2 3)");

    check_equal("#((1 2) (3 4))", "#((1 2) (3 4))");
    check_equal("#((1 2) (3 4) (5 6))", "#((1 2) (3 4) (5 6))");

    return passed;
}

int main(int argc, char **argv) {
    GC_init();
    minim_init();

    return_code = 0;
    log_test("simple", test_read_simple, return_code);
    log_test("pair", test_read_pair, return_code);

    minim_shutdown(0);
    GC_deinit();

    return return_code;
}

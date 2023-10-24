// read.c: reader tests

#include "minim.h"

FILE *istream, *ostream;
int ret_code, passed;

#define load(stream, s) {   \
    fputs(s, stream);       \
    rewind(stream);         \
}

#define log_failed_case(s, expect, actual) {                        \
    printf(" %s => expected: %s, actual: %s\n", s, expect, actual); \
}

#define log_test(name, t) {             \
    if (t() == 1) {                     \
        printf("[ \033[32mPASS\033[0m ] %s\n", name);  \
    } else {                            \
        ret_code = 1;                \
        printf("[ \033[31mFAIL\033[0m ] %s\n", name);  \
    }                                   \
}

void check_equal(const char *in, const char *expect) {
    mobj ip, op, o;
    char *buffer;
    size_t len, read;

    istream = tmpfile();
    ostream = tmpfile();
    load(istream, in);

    ip = Mport(istream, PORT_FLAG_OPEN | PORT_FLAG_READ);
    op = Mport(ostream, PORT_FLAG_OPEN);
    
    o = read_object(ip);
    write_object(op, o);
    len = ftell(ostream);
    fseek(ostream, 0, SEEK_SET);

    buffer = GC_alloc_atomic((len + 1) * sizeof(char));
    read = fread(buffer, 1, len, ostream);
    buffer[len] = '\0';
    if (read != len) {
        fprintf(stderr, "read error occured");
        exit(1);
    }

    if (strcmp(buffer, expect) != 0) {
        log_failed_case(in, expect, buffer);
        passed = 0;
    }

    fclose(istream);
    fclose(ostream);
}

#define check_same(s)   check_equal(s, s)

int test_simple() {
    passed = 1;

    check_same("a");
    check_same("ab");
    check_same("abc");

    check_same("1");
    check_same("12");
    check_same("123");

    check_same("#\\a");
    check_same("#\\b");
    check_same("#\\space");
    check_same("#\\nul");
    check_same("#\\alarm");
    check_same("#\\backspace");
    check_same("#\\tab");
    check_equal("#\\linefeed", "#\\newline");
    check_same("#\\newline");
    check_same("#\\vtab");
    check_same("#\\page");
    check_same("#\\return");
    check_same("#\\esc");
    check_same("#\\space");
    check_same("#\\delete");

    check_same("\"a\"");
    check_same("\"ab\"");
    check_same("\"abc\"");
    check_same("\"abc\\\"\"");

    check_same("\"(a . b)\"");
    check_same("\"(a b . c\"");
    check_same("\"(a b c . d)\"");

    check_same("\"()\"");
    check_same("\"(a)\"");
    check_same("\"(a b)\"");
    check_same("\"(a b c)\"");

    check_same("\"#()\"");
    check_same("\"#(a)\"");
    check_same("\"#(a b)\"");
    check_same("\"#(a b c)\"");

    check_same("\"((1 2) (3 4))\"");
    check_same("\"(1 (2 (3 (4))))\"");

    return passed; 
}

int main() {
    GC_init();
    minim_init();

    log_test("simple", test_simple);

    GC_shutdown();
    return ret_code;
}

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

int main(int argc, char **argv) {
    volatile int stack_top;

    GC_init(((void*) &stack_top));
    minim_boot_init();

    stack_top = 0;
    return_code = stack_top;

    log_test("simple", test_simple);


    GC_finalize();
    return return_code;
}


// test.h: common utilities for unit tests

#include "../src/minim.h"

#define log_test(name, t, return_code) { \
    if (t() == 1) { \
        printf("  [ \033[32mPASS\033[0m ] %s\n", name); \
    } else { \
        return_code = 1; \
        printf("  [ \033[31mFAIL\033[0m ] %s\n", name); \
    } \
}

#define log_failed_case(s, expect, actual) {                        \
    printf(" %s => expected: %s, actual: %s\n", s, expect, actual); \
}

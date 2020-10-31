#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/minim.h"

bool run_test(char *input, char *expected)
{
    char *str;
    bool s;

    str = eval_string(input, INT_MAX);
    s = (strcmp(str, expected) == 0);
    if (!s) printf("FAILED! input: %s, expected: %s, got: %s\n", input, expected, str);
    free(str);

    return s;
}

int main()
{
    bool status = true;

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "\"h\"",                        "\"h\"",
            "\"hello\"",                    "\"hello\"",
            "\"hello world\"",              "\"hello world\"",
            "\"a b c d e\"",                "\"a b c d e\""
        };

        printf("Testing string construction...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(string-append \"a\" \"b\")",              "\"ab\"",
            "(string-append \"hel\" \"lo\")",           "\"hello\"",
            "(string-append \"I a\" \"m h\" \"ere\")",  "\"I am here\""
        };

        printf("Testing 'string-append'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(substring \"hello\" 0)",                  "\"hello\"",
            "(substring \"hello\" 1)",                  "\"ello\"",
            "(substring \"hello\" 0 5)",                "\"hello\"",
            "(substring \"hello\" 1 4)",                "\"ell\"",
            "(substring \"hello\" 2 3)",                "\"l\""
        };

        printf("Testing 'substring'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    return (int)(!status);
}
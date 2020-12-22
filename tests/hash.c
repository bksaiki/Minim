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

bool evaluate(char *input)
{
    char *str = eval_string(input, INT_MAX);
    free(str);

    return true;
}

int main()
{
    bool status = true;

    {
        const int COUNT = 1;
        char strs[2][256] =
        {
            "(hash)",           "hash()",
        };

        printf("Testing hash construction\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(hash-set (hash) 'a 1)",           "hash((a . 1))",
            "(hash-set (hash) 1 'a)",           "hash((1 . a))",
            "(hash-set (hash) 'a (list 1 2))",  "hash((a . (1 2)))"
        };

        printf("Testing 'hash-set'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(hash-remove (hash-set (hash) 'a 1) 'a)",      "hash()",
            "(hash-remove (hash-set (hash) 'a 1) 'b)",      "hash((a . 1))",
        };

        printf("Testing 'hash-remove'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(hash-key? (hash-set (hash) 'a 1) 'a)",      "true",
            "(hash-key? (hash-set (hash) 'a 1) 'b)",      "false",
        };

        printf("Testing 'hash-key?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(hash-set (hash-set (hash-set (hash-set (hash-set (hash-set (hash-set (hash-set (hash) 'a 1) 'b 2) 'c 3) 'd 4) 'e 5) 'f 6) 'g 7) 'h 8)",
        };

        printf("Testing rehashing\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    return (int)(!status);
}
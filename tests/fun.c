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
        char strs[1][256] =
        {
            "(begin (def foo (lambda (x) x))) (foo 1) (foo 'a))"
        };

        printf("Testing lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def ident (lambda (x) x))) (def foo ident) (foo 1) (foo 'a))"
        };

        printf("Testing copied lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def foo (lambda args (apply + args))) (foo 1) (foo 1 2) (foo 1 2 3))",
        };

        printf("Testing rest arguments\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    return (int)(!status);
}
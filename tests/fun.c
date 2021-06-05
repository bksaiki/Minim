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
    return s;
}

bool evaluate(char *input)
{
    eval_string(input, INT_MAX);
    return true;
}

int main()
{
    bool status = true;
    
    GC_init(&status);

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def foo (lambda (x) x)) (foo 1) (foo 'a))"
        };

        printf("Testing lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def ident (lambda (x) x)) (def foo ident) (foo 1) (foo 'a))"
        };

        printf("Testing copied lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def foo (lambda args (apply + args))) (foo 1) (foo 1 2) (foo 1 2 3))",
            "(begin (def foo args (apply + args)) (foo 1) (foo 1 2) (foo 1 2 3))"
        };

        printf("Testing rest arguments\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(begin (def foo (lambda (x . rest) (cons x rest))) (foo 1))",      "'(1)",
            "(begin (def foo (lambda (x . rest) (cons x rest))) (foo 1 2))",    "'(1 2)",
            "(begin (def foo (lambda (x . rest) (cons x rest))) (foo 1 2 3))",  "'(1 2 3)",

            "(begin (def foo (lambda (x y . rest) (cons x rest))) (foo 1 2))",      "'(1 2)",
            "(begin (def foo (lambda (x y . rest) (cons x rest))) (foo 1 2 3))",    "'(1 2 3)",
            "(begin (def foo (lambda (x y . rest) (cons x rest))) (foo 1 2 3 4))",  "'(1 2 3 4)"
        };

        printf("Testing rest arguments\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def foo (lambda (x) (set! x (- x 2)) (set! x (* 2 x)) x)) (foo 6))",
            "(begin (def foo (x) (set! x (- x 2)) (set! x (* 2 x)) x) (foo 6))"
        };

        printf("Testing multi-line body\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    GC_finalize();

    return (int)(!status);
}

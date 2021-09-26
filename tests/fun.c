#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/test/test-common.h"

int main()
{
    bool status = true;
    
    GC_init(&status);
    setup_test_env();

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def-values (foo) (lambda (x) x)) (foo 1) (foo 'a))"
        };

        printf("Testing lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def-values (ident) (lambda (x) x))         \
                    (def-values (foo) ident) (foo 1) (foo 'a))"
        };

        printf("Testing copied lambdas\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def-values (foo) (lambda args (apply + args))) (foo 1) (foo 1 2) (foo 1 2 3))"
        };

        printf("Testing rest arguments\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(begin (def-values (foo) (lambda (x . rest) (cons x rest))) (foo 1))",      "'(1)",
            "(begin (def-values (foo) (lambda (x . rest) (cons x rest))) (foo 1 2))",    "'(1 2)",
            "(begin (def-values (foo) (lambda (x . rest) (cons x rest))) (foo 1 2 3))",  "'(1 2 3)",

            "(begin (def-values (foo) (lambda (x y . rest) (cons x rest))) (foo 1 2))",      "'(1 2)",
            "(begin (def-values (foo) (lambda (x y . rest) (cons x rest))) (foo 1 2 3))",    "'(1 2 3)",
            "(begin (def-values (foo) (lambda (x y . rest) (cons x rest))) (foo 1 2 3 4))",  "'(1 2 3 4)"
        };

        printf("Testing rest arguments\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def-values (foo) (lambda (x) (set! x (- x 2)) (set! x (* 2 x)) x)) (foo 6))"
        };

        printf("Testing multi-line body\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    GC_finalize();

    return (int)(!status);
}

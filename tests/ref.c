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
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(def-values (x) 1) (def-values (y) 2) (cons x y))",
            "(def-values (x) (list 1 2)) (def-values (y) (list 3 4)) (cons x y))"
        };

        printf("Testing 'cons' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 4;
        char strs[4][256] =
        {
            "(begin (def-values (x) (cons 1 2)) (car x))",
            "(begin (def-values (x) (list 1 2 3 4)) (car x))",
            "(begin (def-values (x) (cons 1 2)) (cdr x))",
            "(begin (def-values (x) (list 1 2 3 4)) (cdr x))"
        };

        printf("Testing 'car', 'cdr' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 8;
        char strs[8][256] =
        {
            "(begin (def-values (x) '((1) . 2)) (caar x))",
            "(begin (def-values (x) '((1 . 2) . 3)) (caar x))",
            "(begin (def-values (x) '((1) . 2)) (cdar x))",
            "(begin (def-values (x) '((1 . 2) . 2)) (cdar x))",
            "(begin (def-values (x) '(1 . (2))) (cadr x))",
            "(begin (def-values (x) '(1 . (2 . 3))) (cadr x))",
            "(begin (def-values (x) '(1 . (2))) (cddr x))",
            "(begin (def-values (x) '(1 . (2 . 3))) (cddr x))"
        };

        printf("Testing 'caar', 'cdar', 'cadr', 'cddr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def-values (x) 1) (list x))",
            "(begin (def-values (x) 1) (def-values (y) 2) (list x y))"
        };

        printf("Testing 'list' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def-values (x) (list 1 2)) (append x))",
            "(begin (def-values (x) (list 1 2)) (def-values (y) (list 3 4)) \
                (append x y))",
            "(begin (def-values (x) (list 1 2)) (def-values (y) (list 3 4)) \
                (def-values (z) (list 5 6)) (append x y z))"
        };

        printf("Testing 'append' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def-values (x) (list 1 2)) (reverse x))",
            "(begin (def-values (x) (list 1 2 3 4)) (reverse x))",
        };

        printf("Testing 'reverse' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def-values (x) (list 1 2 3)) (list-ref x 1))"
        };

        printf("Testing 'list-ref' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def-values (x) (list)) (map sqrt x))",
            "(begin (def-values (x) (list 1)) (map sqrt x))",
            "(begin (def-values (x) (list 1 2 3 4)) (map sqrt x))"
        };

        printf("Testing 'map' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def-values (x) (list)) (apply + x))",
            "(begin (def-values (x) (list 1)) (apply + x))",
            "(begin (def-values (x) (list 1 2 3 4)) (apply + x))"
        };

        printf("Testing 'apply' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def-values (x) (list)) (apply + x))",
            "(begin (def-values (x) (list -1)) (apply + x))",
            "(begin (def-values (x) (list 0 1 2)) (apply + x))"
        };

        printf("Testing 'apply' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 4;
        char strs[4][256] =
        {
            "(begin (def-values (x) 'a) (hash-set (hash) x 1))",
            "(begin (def-values (x) 'a) (def-values (y) 1) (hash-set (hash) x y))",
            "(begin (def-values (h) (hash)) (hash-set h 'a 1))",
            "(begin (def-values (h) (hash-set (hash) 'a 1)) (hash-set h 'b 2))"
        };

        printf("Testing 'hash-set' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def-values (h) (hash)) (hash-remove h 'a))",
            "(begin (def-values (h) (hash-set (hash) 'a 1)) (hash-remove h 'a))",
            "(begin (def-values (h) (hash-set (hash) 'a 1)) (hash-remove h 'b))"
        };

        printf("Testing 'hash-remove' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 5;
        char strs[5][256] =
        {
            "(begin (def-values (f) (lambda () 'x)) (f))",
            "(begin (def-values (f) (lambda () 1)) (f))",
            "(begin (def-values (f) (lambda () \"abc\")) (f))",
            "(begin (def-values (f) (lambda () (list))) (f))",
            "(begin (def-values (f) (lambda () (hash))) (f))"
        };

        printf("Testing nullary lambdas (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(begin (def-values (x) 1) (vector x))",             "'#(1)",
            "(begin (def-values (x) 1) (def-values (y) 2) (vector x y))",   "'#(1 2)"
        };

        printf("Testing 'vector (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def-values (v) (vector 1)) (vector-set! v 0 2) v)",         "'#(2)",
            "(begin (def-values (v) (vector 1 2 3)) (vector-set! v 1 1) v)",     "'#(1 1 3)",
            "(begin (def-values (v) (vector 1 2 3)) (vector-set! v 2 1) v)",     "'#(1 2 1)"
        };

        printf("Testing 'vector-set!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def-values (v) (vector)) (vector->list v))",         "'()",
            "(begin (def-values (v) (vector 1)) (vector->list v))",       "'(1)",
            "(begin (def-values (v) (vector 1 2 3)) (vector->list v))",   "'(1 2 3)"
        };

        printf("Testing 'vector->list (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def-values (x) '()) (list->vector x))",         "'#()",
            "(begin (def-values (x) '(1)) (list->vector x))",        "'#(1)",
            "(begin (def-values (x) '(1 2 3)) (list->vector x))",    "'#(1 2 3)"
        };

        printf("Testing 'list->vector (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def-values (v) (vector)) (vector-fill! v 'a) v)",           "'#()",
            "(begin (def-values (v) (vector 1)) (vector-fill! v 'a) v)",         "'#(a)",
            "(begin (def-values (v) (vector 1 2 3)) (vector-fill! v 'a) v)",     "'#(a a a)"
        };

        printf("Testing 'vector-fill!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

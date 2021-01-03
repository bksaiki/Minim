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
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x 1) (def y 2) (cons x y))",
            "(begin (def x (list 1 2)) (def y (list 3 4)) (cons x y))"
        };

        printf("Testing 'cons' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (cons 1 2)) (car x))",
            "(begin (def x (list 1 2 3 4)) (car x))"
        };

        printf("Testing 'car' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (cons 1 2)) (cdr x))",
            "(begin (def x (list 1 2 3 4)) (cdr x))"
        };

        printf("Testing 'cdr' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x 1) (list x))",
            "(begin (def x 1) (def y 2) (list x y))"
        };

        printf("Testing 'list' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (list 1)) (head x))",
            "(begin (def x (list 1 2)) (head x))"
        };

        printf("Testing 'head' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (list 1)) (tail x))",
            "(begin (def x (list 1 2)) (tail x))"
        };

        printf("Testing 'tail' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list 1 2)) (append x))",
            "(begin (def x (list 1 2)) (def y (list 3 4)) (append x y))",
            "(begin (def x (list 1 2)) (def y (list 3 4)) (def z (list 5 6)) (append x y z))"
        };

        printf("Testing 'append' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (list 1 2)) (reverse x))",
            "(begin (def x (list 1 2 3 4)) (reverse x))",
        };

        printf("Testing 'reverse' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(begin (def x (list 1 2 3)) (list-ref x 1))"
        };

        printf("Testing 'list-ref' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (map sqrt x))",
            "(begin (def x (list 1)) (map sqrt x))",
            "(begin (def x (list 1 2 3 4)) (map sqrt x))"
        };

        printf("Testing 'map' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (apply + x))",
            "(begin (def x (list 1)) (apply + x))",
            "(begin (def x (list 1 2 3 4)) (apply + x))"
        };

        printf("Testing 'apply' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (filter positive? x))",
            "(begin (def x (list 1)) (filter positive? x))",
            "(begin (def x (list 0 1 2)) (filter positive? x))"
        };

        printf("Testing 'filter' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (filtern positive? x))",
            "(begin (def x (list -1)) (filtern positive? x))",
            "(begin (def x (list 0 1 2)) (filtern positive? x))"
        };

        printf("Testing 'filtern' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (apply + x))",
            "(begin (def x (list -1)) (apply + x))",
            "(begin (def x (list 0 1 2)) (apply + x))"
        };

        printf("Testing 'apply' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (for ((i x)) i))",
            "(begin (def x (list 1)) (for ((i x)) i))",
            "(begin (def x (list 1 2 3)) (for ((i x)) i))"
        };

        printf("Testing 'for' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def x (list)) (for-list ((i x)) i))",
            "(begin (def x (list 1)) (for-list ((i x)) i))",
            "(begin (def x (list 1 2 3)) (for-list ((i x)) i))"
        };

        printf("Testing 'for-list' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (in-range 3)) (for-list ((i x)) i))",
            "(begin (def x (in-range 1 4)) (for-list ((i x)) i))"
        };

        printf("Testing 'in-range' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(begin (def x (in-range 0)) (sequence->list x))",    "'()",
            "(begin (def x (in-range 5)) (sequence->list x))",    "'(0 1 2 3 4)"    
        };

        printf("Testing 'sequence->list'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[4][256] =
        {
            "(begin (def x 'a) (hash-set (hash) x 1))",
            "(begin (def x 'a) (def y 1) (hash-set (hash) x y))",
            "(begin (def h (hash)) (hash-set h 'a 1))",
            "(begin (def h (hash-set (hash) 'a 1)) (hash-set h 'b 2))"
        };

        printf("Testing 'hash-set' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(begin (def h (hash)) (hash-remove h 'a))",
            "(begin (def h (hash-set (hash) 'a 1)) (hash-remove h 'a))",
            "(begin (def h (hash-set (hash) 'a 1)) (hash-remove h 'b))"
        };

        printf("Testing 'hash-remove' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 5;
        char strs[5][256] =
        {
            "(begin (def f () 'x) (f))",
            "(begin (def f () 1) (f))",
            "(begin (def f () \"abc\") (f))",
            "(begin (def f () (list)) (f))",
            "(begin (def f () (hash)) (f))"
        };

        printf("Testing nullary lambdas (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    return (int)(!status);
}
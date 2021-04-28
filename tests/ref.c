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
        const int COUNT = 4;
        char strs[4][256] =
        {
            "(begin (def x (cons 1 2)) (car x))",
            "(begin (def x (list 1 2 3 4)) (car x))",
            "(begin (def x (cons 1 2)) (cdr x))",
            "(begin (def x (list 1 2 3 4)) (cdr x))"
        };

        printf("Testing 'car', 'cdr' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 8;
        char strs[8][256] =
        {
            "(begin (def x '((1) . 2)) (caar x))",
            "(begin (def x '((1 . 2) . 3)) (caar x))",
            "(begin (def x '((1) . 2)) (cdar x))",
            "(begin (def x '((1 . 2) . 2)) (cdar x))",
            "(begin (def x '(1 . (2))) (cadr x))",
            "(begin (def x '(1 . (2 . 3))) (cadr x))",
            "(begin (def x '(1 . (2))) (cddr x))",
            "(begin (def x '(1 . (2 . 3))) (cddr x))"
        };

        printf("Testing 'caar', 'cdar', 'cadr', 'cddr'\n");
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
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (list 1 2)) (remove 1 x))",
            "(begin (def x (list 1 2 3 4)) (remove 2 x))"
        };

        printf("Testing 'remove' (ref)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(begin (def x (list 1 2)) (member 1 x))",
            "(begin (def x (list 1 2 3 4)) (member 2 x))"
        };

        printf("Testing 'member' (ref)\n");
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

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(begin (def x 1) (vector x))",             "'#(1)",
            "(begin (def x 1) (def y 2) (vector x y))",   "'#(1 2)"
        };

        printf("Testing 'vector (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def v (vector 1)) (vector-set! v 0 2) v)",         "'#(2)",
            "(begin (def v (vector 1 2 3)) (vector-set! v 1 1) v)",     "'#(1 1 3)",
            "(begin (def v (vector 1 2 3)) (vector-set! v 2 1) v)",     "'#(1 2 1)"
        };

        printf("Testing 'vector-set!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def v (vector)) (vector->list v))",         "'()",
            "(begin (def v (vector 1)) (vector->list v))",       "'(1)",
            "(begin (def v (vector 1 2 3)) (vector->list v))",   "'(1 2 3)"
        };

        printf("Testing 'vector->list (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def x '()) (list->vector x))",         "'#()",
            "(begin (def x '(1)) (list->vector x))",        "'#(1)",
            "(begin (def x '(1 2 3)) (list->vector x))",    "'#(1 2 3)"
        };

        printf("Testing 'list->vector (ref)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(begin (def v (vector)) (vector-fill! v 'a) v)",           "'#()",
            "(begin (def v (vector 1)) (vector-fill! v 'a) v)",         "'#(a)",
            "(begin (def v (vector 1 2 3)) (vector-fill! v 'a) v)",     "'#(a a a)"
        };

        printf("Testing 'vector-fill!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    return (int)(!status);
}

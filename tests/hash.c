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
        char strs[2][256] =
        {
            "(hash)",           "'#hash()",
        };

        printf("Testing hash construction\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(hash-set (hash) 'a 1)",           "'#hash((a . 1))",
            "(hash-set (hash) -1 'a)",           "'#hash((-1 . a))",
            "(hash-set (hash) 'a (list 1 2))",  "'#hash((a . (1 2)))",
            "(hash-set (hash) '(1 2) 'a)",      "'#hash(((1 2) . a))",

            "(hash-set (hash-set (hash) 'a 1) 'a 2)",       "'#hash((a . 2))"
        };

        printf("Testing 'hash-set'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(hash-remove (hash-set (hash) 'a 1) 'a)",          "'#hash()",
            "(hash-remove (hash-set (hash) 'a 1) 'b)",          "'#hash((a . 1))",
            "(hash-remove (hash-set (hash) '(1 2) 1) '(1 2))",  "'#hash()",
            "(hash-remove (hash-set (hash) '(1 2) 1) '(2 2))",  "'#hash(((1 2) . 1))",
            "(hash-remove (hash-set (hash) + 1) +)",            "'#hash()",
        };

        printf("Testing 'hash-remove'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(hash-key? (hash-set (hash) 'a 1) 'a)",            "#t",
            "(hash-key? (hash-set (hash) 'a 1) 'b)",            "#f",
            "(hash-key? (hash-set (hash) '(1 2) 1) '(1 2))",    "#t",
            "(hash-key? (hash-set (hash) '(1 2) 1) '(2 2))",    "#f",
            "(hash-key? (hash-set (hash) + 1) +)",              "#t",
            "(hash-key? (hash-set (hash) + 1) -)",              "#f"
        };

        printf("Testing 'hash-key?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[1][256] =
        {
            "(hash-set (hash-set (hash-set (hash-set (hash-set (hash-set (hash-set \
                (hash-set (hash) 'a 1) 'b 2) 'c 3) 'd 4) 'e 5) 'f 6) 'g 7) 'h 8)",
        };

        printf("Testing rehashing\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(begin (def-values (h) (hash)) (hash-set! h 'a 1) h)",                      "'#hash((a . 1))",
            "(begin (def-values (h) (hash)) (hash-set! h -1 'a) h)",                     "'#hash((-1 . a))",
            "(begin (def-values (h) (hash)) (hash-set! h 'a '(1 2)) h)",                 "'#hash((a . (1 2)))",
            "(begin (def-values (h) (hash)) (hash-set! h '(1 2) 'a) h)",                 "'#hash(((1 2) . a))",
            "(begin (def-values (h) (hash)) (hash-set! h 'a 1) (hash-set! h 'a 2) h)",   "'#hash((a . 2))"
        };

        printf("Testing 'hash-set!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(begin (def-values (h) (hash-set (hash) 'a 1)) (hash-remove! h 'a) h)",             "'#hash()",
            "(begin (def-values (h) (hash-set (hash) 'a 1)) (hash-remove! h 'b) h)",             "'#hash((a . 1))",
            "(begin (def-values (h) (hash-set (hash) '(1 2) 1)) (hash-remove! h '(1 2)) h)",     "'#hash()",
            "(begin (def-values (h) (hash-set (hash) '(1 2) 1)) (hash-remove! h '(2 2)) h)",     "'#hash(((1 2) . 1))",
            "(begin (def-values (h) (hash-set (hash) + 1)) (hash-remove! h +) h)",               "'#hash()"
        };

        printf("Testing 'hash-remove!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(hash->list (hash))",                          "'()",
            "(hash->list (hash-set (hash) 'a 1))",          "'((a . 1))",
            "(hash->list (hash-set (hash) '(1 2) 1))",      "'(((1 2) . 1))"
        };

        printf("Testing 'hash->list'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

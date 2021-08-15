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

int main()
{
    bool status = true;

    GC_init(&status);

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "#\\M",             "#\\M",
            "#\\a",             "#\\a",
            "#\\1",             "#\\1"
        };

        printf("Testing character construction\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(char=? #\\A #\\B)",               "#f",
            "(char=? #\\A #\\A)",               "#t",
            "(char=? #\\0 #\\0)",               "#t",
            "(char=? #\\0 #\\a)",               "#f"
        };

        printf("Testing 'char=?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char>? #\\A #\\B)",               "#f",
            "(char>? #\\a #\\b)",               "#f",
            "(char>? #\\A #\\0)",               "#t",
            "(char>? #\\0 #\\a)",               "#f",
            "(char>? #\\a #\\a)",               "#f"
        };

        printf("Testing 'char>?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char<? #\\A #\\B)",               "#t",
            "(char<? #\\a #\\b)",               "#t",
            "(char<? #\\A #\\0)",               "#f",
            "(char<? #\\0 #\\a)",               "#t",
            "(char<? #\\a #\\a)",               "#f"
        };

        printf("Testing 'char<?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char<=? #\\A #\\B)",               "#t",
            "(char<=? #\\a #\\b)",               "#t",
            "(char<=? #\\A #\\0)",               "#f",
            "(char<=? #\\0 #\\a)",               "#t",
            "(char<=? #\\a #\\a)",               "#t"
        };

        printf("Testing 'char<=?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char>=? #\\A #\\B)",               "#f",
            "(char>=? #\\a #\\b)",               "#f",
            "(char>=? #\\A #\\0)",               "#t",
            "(char>=? #\\0 #\\a)",               "#f",
            "(char>=? #\\a #\\a)",               "#t"
        };

        printf("Testing 'char>=?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(char-ci=? #\\A #\\B)",            "#f",
            "(char-ci=? #\\A #\\A)",            "#t",
            "(char-ci=? #\\a #\\a)",            "#t"
        };

        printf("Testing 'char-ci=?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char-ci>? #\\A #\\B)",               "#f",
            "(char-ci>? #\\a #\\b)",               "#f",
            "(char-ci>? #\\a #\\a)",               "#f",
            "(char-ci>? #\\A #\\b)",               "#f",
            "(char-ci>? #\\a #\\B)",               "#f"
        };

        printf("Testing 'char-ci>?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char-ci<? #\\A #\\B)",               "#t",
            "(char-ci<? #\\a #\\b)",               "#t",
            "(char-ci<? #\\a #\\a)",               "#f",
            "(char-ci<? #\\A #\\b)",               "#t",
            "(char-ci<? #\\a #\\B)",               "#t"
        };

        printf("Testing 'char-ci<?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char-ci>=? #\\A #\\B)",               "#f",
            "(char-ci>=? #\\a #\\b)",               "#f",
            "(char-ci>=? #\\a #\\a)",               "#t",
            "(char-ci>=? #\\A #\\b)",               "#f",
            "(char-ci>=? #\\a #\\B)",               "#f"
        };

        printf("Testing 'char-ci>=?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(char-ci<=? #\\A #\\B)",               "#t",
            "(char-ci<=? #\\a #\\b)",               "#t",
            "(char-ci<=? #\\a #\\a)",               "#t",
            "(char-ci<=? #\\A #\\b)",               "#t",
            "(char-ci<=? #\\a #\\B)",               "#t"
        };

        printf("Testing 'char=ci<?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(char-upcase #\\0)",               "#\\0",
            "(char-upcase #\\a)",               "#\\A",
            "(char-upcase #\\A)",               "#\\A"
        };

        printf("Testing 'char-upcase'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(char-downcase #\\0)",               "#\\0",
            "(char-downcase #\\a)",               "#\\a",
            "(char-downcase #\\A)",               "#\\a"
        };

        printf("Testing 'char-downcase'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "\"h\"",                        "\"h\"",
            "\"hello\"",                    "\"hello\"",
            "\"hello world\"",              "\"hello world\"",
            "\"a b c d e\"",                "\"a b c d e\""
        };

        printf("Testing string construction\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(make-string 5)",              "\"?????\"",
            "(make-string 5 #\\a)",          "\"aaaaa\""
        };

        printf("Testing 'make-string'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(string)",                     "\"\"",
            "(string #\\a)",                "\"a\"",
            "(string #\\a #\\b)",           "\"ab\""
        };

        printf("Testing 'string'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(string-length \"\")",             "0",
            "(string-length \"abc\")",          "3"
        };

        printf("Testing 'string-length'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(string-ref \"abc\" 0)",           "#\\a",
            "(string-ref \"abc\" 1)",           "#\\b",
            "(string-ref \"abc\" 2)",           "#\\c"
        };

        printf("Testing 'string-ref'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(def-values (s) \"abcdef\") (string-set! s 0 #\\0) s",      "\"0bcdef\"",
            "(def-values (s) \"abcdef\") (string-set! s 4 #\\1) s",      "\"abcd1f\""
        };

        printf("Testing 'string-set!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(def-values (s) \"\") (string-copy s)",            "\"\"",
            "(def-values (s) \"abcdef\") (string-copy s)",      "\"abcdef\""
        };

        printf("Testing 'string-copy'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(def-values (s) \"\") (string-fill! s #\\0) s",            "\"\"",
            "(def-values (s) \"abcdef\") (string-fill! s #\\0) s",      "\"000000\""
        };

        printf("Testing 'string-fill!'\n");
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

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(string->symbol \"hello\")",           "'hello",
            "(string->symbol \"x y\")",             "'|x y|",
            "(symbol->string 'hello)",              "\"hello\""
        };

        printf("Testing 'string<->symbol'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(format \"hello\")",                   "\"hello\"",
            "(format \"num: ~a\" 1/9)",             "\"num: 1/9\"",
            "(format \"var: ~a\" 'x)",              "\"var: x\"",
            "(format \"list: ~a\" (list 1 2 3))",   "\"list: (1 2 3)\"",
            "(format \"~a -> ~a\" 'a 1)",           "\"a -> 1\""
        };

        printf("Testing 'format'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

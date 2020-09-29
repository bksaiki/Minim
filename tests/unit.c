#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/minim.h"

int main()
{
    bool status = true;

    {
        char* str;
        const int COUNT = 4;
        char strs[10][256] =
        {
            "1",            "1",
            "5/2",          "5/2",
            "'x",            "'x",
            "+",            "<function:+>"
        };

        printf("Testing single expressions...\n");
        for (int i = 0; i < COUNT; ++i)
        {
            bool s;

            str = eval_string(strs[2 * i], INT_MAX);
            s = (strcmp(str, strs[2 * i + 1]) == 0);
            if (!s)     printf("Failed: %s\n", str);
            status &= s;
            free(str);
        }
    }

    {
        char* str;
        const int COUNT = 11;
        char strs[22][256] =
        {
            "(+ 1)",                 "1",
            "(+ 1 2)",               "3",
            "(+ 1 2 3)",             "6",
            "(- 1)",                 "-1",
            "(- 1 2)",               "-1",
            "(- 1 2 3)",             "-4",
            "(* 1)",                 "1",
            "(* 1 2)",               "2",
            "(* 1 2 3)",             "6",
            "(/ 1 2)",               "1/2",
            "(/ 1 0)",               "Division by zero"
        };

        printf("Testing basic math...\n");
        for (int i = 0; i < COUNT; ++i)
        {
            bool s;

            str = eval_string(strs[2 * i], INT_MAX);
            s = (strcmp(str, strs[2 * i + 1]) == 0);
            if (!s)     printf("FAILED! expected: %s got: %s\n", strs[2 * i + 1], str);
            status &= s;
            free(str);
        }
    }

    {
        char* str;
        const int COUNT = 7;
        char strs[14][256] =
        {
            "(list)",                   "'()",
            "(list 1)",                 "'(1)",
            "(list 1 2)",               "'(1 2)",
            "(list 1 2 3 4)",           "'(1 2 3 4)",
            "(list (list 1 2) 3)",      "'((1 2) 3)",
            "(list 1 (list 2 3))",      "'(1 (2 3))",
            "(list (list (list 1)))",   "'(((1)))",
        };

        printf("Testing list construction...\n");
        for (int i = 0; i < COUNT; ++i)
        {
            bool s;

            str = eval_string(strs[2 * i], INT_MAX);
            s = (strcmp(str, strs[2 * i + 1]) == 0);
            if (!s)     printf("FAILED! expected: %s got: %s\n", strs[2 * i + 1], str);
            status &= s;
            free(str);
        }
    }

    {
        char* str;
        const int COUNT = 10;
        char strs[20][256] =
        {
            "(head (list 1))",               "1",
            "(head (list 1 2))",            "1",
            "(tail (list 1))",              "1",
            "(tail (list 1 2))",            "2",
            "(length (list))",              "0",
            "(length (list 1))",            "1",
            "(length (list 1 2))",          "2",
            "(list-ref (list 1) 0)",        "1",
            "(list-ref (list 1 2) 0)",      "1",
            "(list-ref (list 1 2) 1)",      "2"
        };

        printf("Testing list accessors...\n");
        for (int i = 0; i < COUNT; ++i)
        {
            bool s;

            str = eval_string(strs[2 * i], INT_MAX);
            s = (strcmp(str, strs[2 * i + 1]) == 0);
            if (!s)     printf("FAILED! expected: %s got: %s\n", strs[2 * i + 1], str);
            status &= s;
            free(str);
        }
    }

    return (int)(!status);
}
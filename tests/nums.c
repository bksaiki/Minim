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

int main()
{
    bool status = true;

    {
        const int COUNT = 10;
        char strs[20][256] =
        {
            "1",            "1",
            "5/2",          "5/2",
            "-10",          "-10",
            "-29/4",        "-29/4",
            "1.1",          "1.1",
            "1e50",         "1e+50",
            "-5.1",         "-5.1",
            "-1e50",        "-1e+50",
            "1e-50",        "1e-50",
            "1e+50",        "1e+50"
        };

        printf("Testing numbers\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(= 1 1)",          "true",
            "(= 1.0 1.0)",      "true",
            "(= 1 1.0)",        "true",
            "(= 1 2)",          "false",
            "(= 1.0 2.0)",      "false"
        };

        printf("Testing equality\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(> 1 1)",              "false",
            "(> 1.0 1.0)",          "false",
            "(> 3 2)",              "true",
            "(> 3.0 2.0)",          "true",
            "(> 2 3)",              "false",
            "(> 2.0 3.0)",          "false",
            "(> 28/7 -10.0)",       "true",
            "(> 10.0 -28/7)",       "true"
        };

        printf("Testing '>'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(< 1 1)",              "false",
            "(< 1.0 1.0)",          "false",
            "(< 3 2)",              "false",
            "(< 3.0 2.0)",          "false",
            "(< 2 3)",              "true",
            "(< 2.0 3.0)",          "true",
            "(< 28/7 -10.0)",       "false",
            "(< 10.0 -28/7)",       "false"
        };

        printf("Testing '<'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(>= 1 1)",              "true",
            "(>= 1.0 1.0)",          "true",
            "(>= 3 2)",              "true",
            "(>= 3.0 2.0)",          "true",
            "(>= 2 3)",              "false",
            "(>= 2.0 3.0)",          "false",
            "(>= 28/7 -10.0)",       "true",
            "(>= 10.0 -28/7)",       "true"
        };

        printf("Testing '>='\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(<= 1 1)",              "true",
            "(<= 1.0 1.0)",          "true",
            "(<= 3 2)",              "false",
            "(<= 3.0 2.0)",          "false",
            "(<= 2 3)",              "true",
            "(<= 2.0 3.0)",          "true",
            "(<= 28/7 -10.0)",       "false",
            "(<= 10.0 -28/7)",       "false"
        };

        printf("Testing '<='\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(exact 1)",                        "1",
            "(exact 1.0)",                      "1",
            "(begin (def n 1) (exact n))",      "1",
            "(begin (def n 1.0) (exact n))",    "1"
        };

        printf("Testing 'exact'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(inexact 1)",                      "1.0",
            "(inexact 1.0)",                    "1.0",
            "(begin (def n 1) (inexact n))",    "1.0",
            "(begin (def n 1.0) (inexact n))",  "1.0"
        };

        printf("Testing 'inexact'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
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
            "(/ 1 0)",               "nan.0"
        };

        printf("Testing basic math\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(sqrt 2)",             "1.414213562373095",
            "(sqrt 2.0)",           "1.414213562373095",
            "(sqrt 9)",             "3",
            "(sqrt 9.0)",           "3.0",
            "(sqrt -1)",            "nan.0"
        };

        printf("Testing 'sqrt'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(exp 1)",             "2.718281828459045",
            "(exp 1.0)",           "2.718281828459045",
            "(exp 0)",             "1",
            "(exp 0.0)",           "1.0",
            "(exp 100)",           "2.688117141816136e+43",
            "(exp -100)",          "3.720075976020836e-44"
        };

        printf("Testing 'exp'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(log 1)",              "0",
            "(log 1.0)",            "0.0",
            "(log 10)",             "2.302585092994046",
            "(log 10.0)",           "2.302585092994046",
            "(log 1/5)",            "-1.609437912434101",
            "(log -1)",             "nan.0"
        };

        printf("Testing 'log'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 10;
        char strs[20][256] =
        {
            "(pow 0 1)",            "0",
            "(pow 0.0 1)",          "0.0",
            "(pow 0 0)",            "1",
            "(pow 0.0 0)",          "1.0",
            "(pow 3 4)",            "81",
            "(pow 3.0 4)",          "81.0",
            "(pow 3.0 4.0)",        "81.0",
            "(pow 3 4.0)",          "81.0",
            "(pow 2 -5)",           "1/32",
            "(pow 2 -5.0)",         "0.03125"
        };

        printf("Testing 'pow'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    return (int)(!status);
}
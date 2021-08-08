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
        const int COUNT = 3;
        char strs[6][256] =
        {
            "inf",              "inf",
            "-inf",             "-inf",
            "nan",              "nan"
        };

        printf("Testing constants\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(= 1 1)",          "#t",
            "(= 1.0 1.0)",      "#t",
            "(= 1 1.0)",        "#t",
            "(= 1 2)",          "#f",
            "(= 1.0 2.0)",      "#f"
        };

        printf("Testing equality\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(> 1 1)",              "#f",
            "(> 1.0 1.0)",          "#f",
            "(> 3 2)",              "#t",
            "(> 3.0 2.0)",          "#t",
            "(> 2 3)",              "#f",
            "(> 2.0 3.0)",          "#f",
            "(> 28/7 -10.0)",       "#t",
            "(> 10.0 -28/7)",       "#t"
        };

        printf("Testing '>'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(< 1 1)",              "#f",
            "(< 1.0 1.0)",          "#f",
            "(< 3 2)",              "#f",
            "(< 3.0 2.0)",          "#f",
            "(< 2 3)",              "#t",
            "(< 2.0 3.0)",          "#t",
            "(< 28/7 -10.0)",       "#f",
            "(< 10.0 -28/7)",       "#f"
        };

        printf("Testing '<'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(>= 1 1)",              "#t",
            "(>= 1.0 1.0)",          "#t",
            "(>= 3 2)",              "#t",
            "(>= 3.0 2.0)",          "#t",
            "(>= 2 3)",              "#f",
            "(>= 2.0 3.0)",          "#f",
            "(>= 28/7 -10.0)",       "#t",
            "(>= 10.0 -28/7)",       "#t"
        };

        printf("Testing '>='\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(<= 1 1)",              "#t",
            "(<= 1.0 1.0)",          "#t",
            "(<= 3 2)",              "#f",
            "(<= 3.0 2.0)",          "#f",
            "(<= 2 3)",              "#t",
            "(<= 2.0 3.0)",          "#t",
            "(<= 28/7 -10.0)",       "#f",
            "(<= 10.0 -28/7)",       "#f"
        };

        printf("Testing '<='\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(integer? 1)",                         "#t",
            "(integer? 1.0)",                       "#t",
            "(integer? 'str)",                      "#f",
            "(begin (def-values (n) 1) (integer? n))",       "#t",
            "(begin (def-values (n) 1.0) (integer? n))",     "#t",
            "(begin (def-values (n) 'str) (integer? n))",    "#f"
        };

        printf("Testing 'integer?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(exact 1)",                        "1",
            "(exact 1.0)",                      "1",
            "(begin (def-values (n) 1) (exact n))",      "1",
            "(begin (def-values (n) 1.0) (exact n))",    "1"
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
            "(begin (def-values (n) 1) (inexact n))",    "1.0",
            "(begin (def-values (n) 1.0) (inexact n))",  "1.0"
        };

        printf("Testing 'inexact'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 12;
        char strs[24][256] =
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
            "(/ 2)",                 "1/2",
            "(/ 1 2)",               "1/2",
            "(/ 1 0)",               "nan"
        };

        printf("Testing basic math\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(max 1)",                      "1",
            "(max 2 3 1)",                  "3",
            "(max -5 4 6 2 1)",             "6",
            "(min 1)",                      "1",
            "(min 2 3 1)",                  "1",
            "(min -5 4 6 2 1)",             "-5"
        };

        printf("Testing 'max' and 'min'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(modulo 1 1)",                "0",
            "(modulo 27 6)",               "3",
            "(modulo 10 3)",               "1",
            "(modulo -10 3)",              "2",
            "(modulo -10 -3)",             "-1",
            "(modulo 10 -3)",              "-2"
        };

        printf("Testing 'modulo'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(remainder 1 1)",                "0",
            "(remainder 27 6)",               "3",
            "(remainder 10 3)",               "1",
            "(remainder -10 3)",              "-1",
            "(remainder -10 -3)",             "-1",
            "(remainder 10 -3)",              "1"
        };

        printf("Testing 'remainder'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(quotient 1 1)",                "1",
            "(quotient 27 6)",               "4",
            "(quotient 10 3)",               "3",
            "(quotient -10 3)",              "-3",
            "(quotient -10 -3)",             "3",
            "(quotient 10 -3)",              "-3"
        };

        printf("Testing 'quotient'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(numerator (/ 6 4))",          "3",
            "(numerator (/ 7 4))",          "7",
            "(numerator 3.5)",              "7.0"
        };

        printf("Testing 'numerator'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(denominator (/ 6 4))",          "2",
            "(denominator (/ 7 4))",          "4",
            "(denominator 3.5)",              "2.0"
        };

        printf("Testing 'denominator'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(floor -4.3)",             "-5.0",
            "(floor 3.5)",              "3.0",
            "(floor -5/3)",             "-2",
            "(floor 3/2)",              "1"   
        };

        printf("Testing 'floor'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(ceil -4.3)",             "-4.0",
            "(ceil 3.5)",              "4.0",
            "(ceil -5/3)",             "-1",
            "(ceil 3/2)",              "2"   
        };

        printf("Testing 'ceil'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(trunc -4.3)",             "-4.0",
            "(trunc 3.5)",              "3.0",
            "(trunc -5/3)",             "-1",
            "(trunc 3/2)",              "1"   
        };

        printf("Testing 'trunc'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(round -4.3)",             "-4.0",
            "(round 3.5)",              "4.0",
            "(round -5/3)",             "-2",
            "(round 3/2)",              "2"   
        };

        printf("Testing 'round'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(gcd 1 1)",                "1",
            "(gcd 27 6)",               "3",
            "(gcd 10 25)",              "5",
            "(gcd 101 15)",             "1"
        };

        printf("Testing 'gcd'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(lcm 1 1)",                "1",
            "(lcm 27 6)",               "54",
            "(lcm 10 25)",              "50",
            "(lcm 101 15)",             "1515"
        };

        printf("Testing 'lcm'\n");
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
            "(sqrt -1)",            "nan"
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
            "(log -1)",             "nan"
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

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(sin 0)",              "0",
            "(sin 0.0)",            "0.0",
            "(cos 0)",              "1",
            "(cos 0.0)",            "1.0",
            "(tan 0)",              "0",
            "(tan 0.0)",            "0.0"
        };

        printf("Testing 'sin', 'cos', 'tan'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(asin 0)",             "0",
            "(asin 0.0)",           "0.0",
            "(acos 1)",             "0",
            "(acos 1.0)",           "0.0",
            "(atan 0)",             "0",
            "(atan 0.0)",           "0.0"
        };

        printf("Testing 'asin', 'acos', 'atan'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

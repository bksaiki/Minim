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
        const int COUNT = 4;
        char strs[8][256] =
        {
            "1",            "1",
            "5/2",          "5/2",
            "'x",            "'x",
            "+",            "<function:+>"
        };

        printf("Testing single expressions...\n");
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
            "(/ 1 0)",               "Division by zero"
        };

        printf("Testing basic math...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 9;
        char strs[18][256] =
        {
            "(list)",                   "'()",
            "(list 1)",                 "'(1)",
            "(list 1 2)",               "'(1 2)",
            "(list 1 2 3 4)",           "'(1 2 3 4)",
            "(list 'a 'b 'c)",          "'(a b c)",
            "(list (list 1 2) 3)",      "'((1 2) 3)",
            "(list 1 (list 2 3))",      "'(1 (2 3))",
            "(list (list 'a 'b))",      "'((a b))",
            "(list (list (list 1)))",   "'(((1)))",
        };

        printf("Testing list construction...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
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
            "(list-ref (list 1 2) 1)",      "2",
        };

        printf("Testing list accessors...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(cons 1 2)",                       "'(1 . 2)",
            "(cons 1 (cons 2 3))",              "'(1 2 . 3)",
            "(cons 1 (cons 2 (cons 3 4)))",     "'(1 2 3 . 4)",
            "(cons 1 (list))",                  "'(1)",
            "(cons 1 (list 1 2))",              "'(1 1 2)"
        };

        printf("Testing pairs...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(def x 1)",                            "<void>",
            "(def y (+ 1 2))",                      "<void>",
            "(def z (+ 1 (* 2 3)))",                "<void>",
            "(def nullary () 1)",                   "<void>",
            "(def add1 (x) (+ x 1))",               "<void>",
            "(def hyp2 (x y) (+ (* x x) (* y y)))", "<void>",
        };

        printf("Testing 'def'...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 10;
        char strs[20][256] =
        {
            "(let () 1)",                       "1",
            "(let ((x 1)) x)",                  "1",
            "(let ((x 1)) (+ x 1))",            "2",
            "(let ((x 1) (y 2)) (+ x y))",      "3",
            "(let* () 1)",                      "1",
            "(let* ((x 1)) x)",                 "1",
            "(let* ((x 1)) (+ x 1))",           "2",
            "(let* ((x 1) (y 2)) (+ x y))",     "3",  
            "(let* ((x 1) (y x)) y)",           "1",
            "(let* ((x 1) (y x)) (+ x y))",     "2"                    
        };

        printf("Testing 'let/let*'...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(lambda () 1)",                                "<function:?>",
            "(lambda (x) (+ x 1))",                         "<function:?>",
            "(lambda (x y) (+ (* x x) (* y y)))",           "<function:?>"
        };

        printf("Testing 'lambda'...\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(apply + (list 1 2 3 4))",                     "10",
            "(map (lambda (x) (+ x 1)) (list 1 2 3))",      "'(2 3 4)",
            "(append (list 1 2) (list 3 4) (list 5 6))",    "'(1 2 3 4 5 6)",
            "(reverse (list 1 2 3 4 5 6))",                 "'(6 5 4 3 2 1)"
        };

        printf("Test list modifiers..\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    return (int)(!status);
}
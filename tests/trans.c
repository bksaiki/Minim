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
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(def-syntax foo (syntax-rules () [(_ a) a]))",
            "(def-syntax foo (syntax-rules () [(_ a) 1] [(_ a b) 2]))",
            "(def-syntax foo (syntax-rules (c d) [(_ a) 1] [(_ a b) 2]))"
        };

        printf("Define transforms\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(def-syntax foo        \
              (syntax-rules ()      \
               [(_ a b c) 3]        \
               [(_ a b) 2]          \
               [(_ a) 1]))          \
             (foo a b c)",
            "3",

            "(def-syntax foo        \
              (syntax-rules ()      \
               [(_ a b c) 3]        \
               [(_ a b) 2]          \
               [(_ a) 1]))          \
             (foo a b)",
            "2",

            "(def-syntax foo        \
              (syntax-rules ()      \
               [(_ a b c) 3]        \
               [(_ a b) 2]          \
               [(_ a) 1]))          \
             (foo a)",
             "1"
        };

        printf("Basic transforms\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 7;
        char strs[14][256] =
        {
            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a b c) (list c b a)]     \
               [(_ a b) (list b a)]         \
               [(_ a) (list a)]))           \
             (foo 1 2 3)",
            "'(3 2 1)",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a b c) (list c b a)]     \
               [(_ a b) (list b a)]         \
               [(_ a) (list a)]))           \
             (foo 1 2)",
            "'(2 1)",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a b c) (list c b a)]     \
               [(_ a b) (list b a)]         \
               [(_ a) (list a)]))           \
             (foo 1)",
            "'(1)",

            "(def-syntax foo              \
              (syntax-rules ()            \
               [(_ a b c) '(a b c)]))     \
             (foo 1 2 3)",
            "'(1 2 3)",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ #(a b c)) '(a b c)]))    \
             (foo #(1 2 3))",
            "'(1 2 3)",

            "(def-syntax foo                  \
              (syntax-rules ()                \
               [(_ (a b) c d) '(a b c d)]))   \
             (foo (1 2) 3 4)",
            "'(1 2 3 4)",

            "(def-syntax foo              \
              (syntax-rules ()            \
               [(_ a b c) #(a b c)]))     \
             (foo 1 2 3)",
            "'#(1 2 3)"
        };

        printf("Transforms w/ variables\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 9;
        char strs[18][256] =
        {
            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) (list a ...)]))   \
             (foo)",
            "'()",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) (list a ...)]))   \
             (foo 1)",
            "'(1)",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) (list a ...)]))   \
             (foo 1 2)",
            "'(1 2)",

            "(def-syntax bar                           \
              (syntax-rules ()                         \
               [(_ a b ...) (list a (list b ...))]))   \
             (bar 1)",
            "'(1 ())",

            "(def-syntax bar                           \
              (syntax-rules ()                         \
               [(_ a b ...) (list a (list b ...))]))   \
             (bar 1 2)",
            "'(1 (2))",

            "(def-syntax bar                           \
              (syntax-rules ()                         \
               [(_ a b ...) (list a (list b ...))]))   \
             (bar 1 2 3)",
            "'(1 (2 3))",

            "(def-syntax baz                                \
              (syntax-rules ()                              \
               [(_ a b c ...) (list a b (list c ...))]))    \
             (baz 1 2)",
            "'(1 2 ())",

            "(def-syntax baz                                \
              (syntax-rules ()                              \
               [(_ a b c ...) (list a b (list c ...))]))    \
             (baz 1 2 3)",
            "'(1 2 (3))",

            "(def-syntax baz                                \
              (syntax-rules ()                              \
               [(_ a b c ...) (list a b (list c ...))]))    \
             (baz 1 2 3 4)",
            "'(1 2 (3 4))",
        };

        printf("Transforms w/ pattern variables (list)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 9;
        char strs[18][256] =
        {
            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) '#(a ...)]))      \
             (foo)",
            "'#()",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) #(a ...)]))       \
             (foo 1)",
            "'#(1)",

            "(def-syntax foo                \
              (syntax-rules ()              \
               [(_ a ...) #(a ...)]))       \
             (foo 1 2)",
            "'#(1 2)",

            "(def-syntax bar                    \
              (syntax-rules ()                  \
               [(_ a b ...) #(a #(b ...))]))    \
             (bar 1)",
            "'#(1 #())",

            "(def-syntax bar                    \
              (syntax-rules ()                  \
               [(_ a b ...) #(a #(b ...))]))    \
             (bar 1 2)",
            "'#(1 #(2))",

            "(def-syntax bar                    \
              (syntax-rules ()                  \
               [(_ a b ...) #(a #(b ...))]))    \
             (bar 1 2 3)",
            "'#(1 #(2 3))",

            "(def-syntax baz                          \
              (syntax-rules ()                        \
               [(_ a b c ...) #(a b #(c ...))]))      \
             (baz 1 2)",
            "'#(1 2 #())",

            "(def-syntax baz                          \
              (syntax-rules ()                        \
               [(_ a b c ...) #(a b #(c ...))]))      \
             (baz 1 2 3)",
            "'#(1 2 #(3))",

            "(def-syntax baz                          \
              (syntax-rules ()                        \
               [(_ a b c ...) #(a b #(c ...))]))      \
             (baz 1 2 3 4)",
            "'#(1 2 #(3 4))"
        };

        printf("Transforms w/ pattern variables (vector)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "(def-syntax foo                          \
              (syntax-rules ()                        \
               [(_ (a b) ...) '((a ...) (b ...))]))   \
             (foo (1 2))",
            "'((1) (2))",

            "(def-syntax foo                          \
              (syntax-rules ()                        \
               [(_ (a b) ...) '((a ...) (b ...))]))   \
             (foo (1 2) (3 4))",
            "'((1 3) (2 4))"

            // "(def-syntax foo                                          
            //   (syntax-rules ()                                        
            //    [(_ (a b c ...) ...) '((a ...) (b ...) (c ...) ...)])) 
            //  (foo (1 2))",
            // "'((1) (2) ())",

            // "(def-syntax foo                                          
            //   (syntax-rules ()                                        
            //    [(_ (a b c ...) ...) '((a ...) (b ...) (c ...) ...)])) 
            //  (foo (1 2) (1 2 3))",
            // "'((1 1) (2 2) () (3))",
        };

        printf("Transforms w/ nested pattern variables\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

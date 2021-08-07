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
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(syntax 1)",           "<syntax:1>",
            "(syntax a)",           "<syntax:a>",
            "(syntax (1 . 2))",     "<syntax:(1 . 2)>",
            "(syntax (1 2 3))",     "<syntax:(1 2 3)>",
            "(syntax #(1 2 3))",    "<syntax:#(1 2 3)>"
        };

        printf("Testing 'syntax'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "#'1",           "<syntax:1>",
            "#'a",           "<syntax:a>",
            "#'(1 . 2)",     "<syntax:(1 . 2)>",
            "#'(1 2 3)",     "<syntax:(1 2 3)>",
            "#'#(1 2 3)",    "<syntax:#(1 2 3)>"
        };

        printf("Testing 'syntax (shorthand)'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(datum->syntax 1)",                "<syntax:1>",
            "(datum->syntax 'a)",               "<syntax:a>",
            "(datum->syntax '(1 . 2))",         "<syntax:(1 . 2)>",
            "(datum->syntax '(1 2 3))",         "<syntax:(1 2 3)>",
            "(datum->syntax '(1 2 . 3))",       "<syntax:(1 2 . 3)>",
            "(datum->syntax #(1 2 3))",         "<syntax:#(1 2 3)>"
        };

        printf("Testing 'datum->syntax'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(syntax-case #'1 ()  \
              [_ 1])",
            "1",

            "(syntax-case #'1 ()  \
              [_ #'1])",
            "<syntax:1>",
            
            "(syntax-case #'(1 2 3) ()      \
              [(_ a b) #'(a b)])",
            "<syntax:(2 3)>",

            "(syntax-case #'(1 a 3) (a)     \
              [(_ a b) #'(a b)])",
            "<syntax:(a 3)>",

            "(syntax-case #'(1 2 3) ()      \
              [(_ a b) (cons 1 #'(a b))])",
            "'(1 . <syntax:(2 3)>)",

            "(syntax-case #'(foo x y) ()    \
              [(_ a b) #'(let ([t a])       \
                          (set! a b)        \
                          (set! b t))])",
            "<syntax:(let ((t x)) (set! x y) (set! y t))>"
        };

        printf("Testing 'syntax-case'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[3][256] =
        {
            "(def-syntaxes (foo)          \
              (lambda (stx)               \
               (syntax-case stx ()        \
                [(_ a) #'a])))",
            "(def-syntaxes (foo)          \
              (lambda (stx)               \
               (syntax-case stx ()        \
                [(_ a) #'1]               \
                [(_ a b) #'2])))",
            "(def-syntaxes (foo)          \
              (lambda (stx)               \
               (syntax-case stx (c d)     \
                [(_ a) #'1]               \
                [(_ a b) #'2])))"
        };

        printf("Define transforms\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(def-syntaxes (foo)        \
              (lambda (stx)             \
               (syntax-case stx ()      \
                [(_ a b c) #'3]         \
                [(_ a b) #'2]           \
                [(_ a) #'1])))          \
             (foo a b c)",
            "3",

            "(def-syntaxes (foo)        \
              (lambda (stx)             \
               (syntax-case stx ()      \
                [(_ a b c) #'3]         \
                [(_ a b) #'2]           \
                [(_ a) #'1])))          \
             (foo a b)",
            "2",

            "(def-syntaxes (foo)        \
              (lambda (stx)             \
               (syntax-case stx ()      \
                [(_ a b c) #'3]         \
                [(_ a b) #'2]           \
                [(_ a) #'1])))          \
             (foo a)",
             "1"
        };

        printf("Basic transforms\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 7;
        char strs[14][512] =
        {
            "(def-syntaxes (foo)              \
              (lambda (stx)                   \
               (syntax-case stx ()            \
                [(_ a b c) #'(list c b a)]    \
                [(_ a b) #'(list b a)]        \
                [(_ a) #'(list a)])))         \
             (foo 1 2 3)",
            "'(3 2 1)",

            "(def-syntaxes (foo)              \
              (lambda (stx)                   \
               (syntax-case stx ()            \
                [(_ a b c) #'(list c b a)]    \
                [(_ a b) #'(list b a)]        \
                [(_ a) #'(list a)])))         \
             (foo 1 2)",
            "'(2 1)",

            "(def-syntaxes (foo)              \
              (lambda (stx)                   \
               (syntax-case stx ()            \
                [(_ a b c) #'(list c b a)]    \
                [(_ a b) #'(list b a)]        \
                [(_ a) #'(list a)])))         \
             (foo 1)",
            "'(1)",

            "(def-syntaxes (foo)              \
              (lambda (stx)                   \
               (syntax-case stx ()            \
                [(_ a b c) #''(a b c)])))     \
             (foo 1 2 3)",
            "'(1 2 3)",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ #(a b c)) #''(a b c)])))    \
             (foo #(1 2 3))",
            "'(1 2 3)",

            "(def-syntaxes (foo)                    \
              (lambda (stx)                         \
               (syntax-case stx ()                  \
                [(_ (a b) c d) #''(a b c d)])))     \
             (foo (1 2) 3 4)",
            "'(1 2 3 4)",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a b c) #'#(a b c)])))       \
             (foo 1 2 3)",
            "'#(1 2 3)"
        };

        printf("Transforms w/ variables\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 9;
        char strs[18][512] =
        {
            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'(list a ...)])))   \
             (foo)",
            "'()",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'(list a ...)])))   \
             (foo 1)",
            "'(1)",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'(list a ...)])))   \
             (foo 1 2)",
            "'(1 2)",

            "(def-syntaxes (bar)                          \
              (lambda (stx)                               \
               (syntax-case stx ()                        \
                [(_ a b ...) #'(list a (list b ...))])))  \
             (bar 1)",
            "'(1 ())",

            "(def-syntaxes (bar)                          \
              (lambda (stx)                               \
               (syntax-case stx ()                        \
                [(_ a b ...) #'(list a (list b ...))])))  \
             (bar 1 2)",
            "'(1 (2))",

            "(def-syntaxes (bar)                            \
              (lambda (stx)                                 \
               (syntax-case stx ()                          \
                [(_ a b ...) #'(list a (list b ...))])))    \
             (bar 1 2 3)",
            "'(1 (2 3))",

            "(def-syntaxes (baz)                                \
              (lambda (stx)                                     \
               (syntax-case stx ()                              \
                [(_ a b c ...) #'(list a b (list c ...))])))    \
             (baz 1 2)",
            "'(1 2 ())",

            "(def-syntaxes (baz)                                \
              (lambda (stx)                                     \
               (syntax-case stx ()                              \
                [(_ a b c ...) #'(list a b (list c ...))])))    \
             (baz 1 2 3)",
            "'(1 2 (3))",

            "(def-syntaxes (baz)                              \
              (lambda (stx)                                   \
               (syntax-case stx ()                            \
                [(_ a b c ...) #'(list a b (list c ...))])))  \
             (baz 1 2 3 4)",
            "'(1 2 (3 4))",
        };

        printf("Transforms w/ pattern variables (list)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 9;
        char strs[18][512] =
        {
            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'#(a ...)])))       \
             (foo)",
            "'#()",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'#(a ...)])))       \
             (foo 1)",
            "'#(1)",

            "(def-syntaxes (foo)                \
              (lambda (stx)                     \
               (syntax-case stx ()              \
                [(_ a ...) #'#(a ...)])))       \
             (foo 1 2)",
            "'#(1 2)",

            "(def-syntaxes (bar)                    \
              (lambda (stx)                         \
               (syntax-case stx ()                  \
                [(_ a b ...) #'#(a #(b ...))])))    \
             (bar 1)",
            "'#(1 #())",

            "(def-syntaxes (bar)                    \
              (lambda (stx)                         \
               (syntax-case stx ()                  \
                [(_ a b ...) #'#(a #(b ...))])))    \
             (bar 1 2)",
            "'#(1 #(2))",

            "(def-syntaxes (bar)                    \
              (lambda (stx)                         \
               (syntax-case stx ()                  \
                [(_ a b ...) #'#(a #(b ...))])))    \
             (bar 1 2 3)",
            "'#(1 #(2 3))",

            "(def-syntaxes (baz)                        \
              (lambda (stx)                           \
               (syntax-case stx ()                    \
                [(_ a b c ...) #'#(a b #(c ...))])))  \
             (baz 1 2)",
            "'#(1 2 #())",

            "(def-syntaxes (baz)                        \
              (lambda (stx)                             \
               (syntax-case stx ()                      \
                [(_ a b c ...) #'#(a b #(c ...))])))    \
             (baz 1 2 3)",
            "'#(1 2 #(3))",

            "(def-syntaxes (baz)                        \
              (lambda (stx)                             \
               (syntax-case stx ()                      \
                [(_ a b c ...) #'#(a b #(c ...))])))    \
             (baz 1 2 3 4)",
            "'#(1 2 #(3 4))"
        };

        printf("Transforms w/ pattern variables (vector)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][512] =
        {
            "(def-syntaxes (foo)                            \
              (lambda (stx)                                 \
               (syntax-case stx ()                          \
                [(_ (a b) ...) #''((a ...) (b ...))])))     \
             (foo (1 2))",
            "'((1) (2))",

            "(def-syntaxes (foo)                            \
              (lambda (stx)                                 \
               (syntax-case stx ()                          \
                [(_ (a b) ...) #''((a ...) (b ...))])))     \
             (foo (1 2) (3 4))",
            "'((1 3) (2 4))",

            "(def-syntaxes (foo)                            \
              (lambda (stx)                                 \
               (syntax-case stx ()                          \
                [(_ (a b c ...) ...)                        \
                  #''((a ...) (b ...) (c ...) ...)])))      \
             (foo (1 2))",
            "'((1) (2) ())",

            "(def-syntaxes (foo)                            \
              (lambda (stx)                                 \
               (syntax-case stx ()                          \
                [(_ (a b c ...) ...)                        \
                  #''((a ...) (b ...) (c ...) ...)])))      \
             (foo (1 2) (1 2 3))",
            "'((1 1) (2 2) () (3))"
        };

        printf("Transforms w/ nested pattern variables\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[1][1024] =
        {
            "(def-syntaxes (foo)                                  \
              (lambda (stx)                                       \
               (syntax-case stx ()                                \
                [(_ a b) #'(printf \"~a\n\" (vector a b))])))     \
             (def-syntaxes (foo)                                  \
              (lambda (stx)                                       \
               (syntax-case stx ()                                \
                [(_ [a b] ...) (begin (foo a b) ...)])))"
        };

        printf("Transforms w/ nested syntax\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    GC_finalize();

    return (int)(!status);
}

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
        char strs[6][256] =
        {
            "'x",           "'x",
            "+",            "<function:+>",
            "1",            "1"
        };

        printf("Testing single expressions\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(def-values (x) 1)",                                       "<void>",
            "(def-values (y) (+ 1 2))",                                 "<void>",
            "(def-values (x y) (values 1 (* 2 3)))",                    "<void>",
            "(def-values (f g) (values (lambda () 1) (lambda () 2)))",  "<void>"
        };

        printf("Testing 'def-values'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] = 
        {
            "(equal? 'x 'x)",                   "#t",
            "(equal? 'x 'y)",                   "#f",
            "(equal? 5 5)",                     "#t",
            "(equal? 5 4)",                     "#f",
            "(equal? 5 5.0)",                   "#t",
            "(equal? \"x\" \"x\")",             "#t",
            "(equal? \"x\" \"y\")",             "#f",
            "(equal? #\\a #\\a)",               "#t"
        };

        printf("Testing 'equal?' (primitive)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] = 
        {
            "(eqv? 'x 'x)",                   "#t",
            "(eqv? 'x 'y)",                   "#f",
            "(eqv? 5 5)",                     "#t",
            "(eqv? 5 4)",                     "#f",
            "(eqv? 5 5.0)",                   "#f",
            "(eqv? \"x\" \"x\")",             "#f",
            "(eqv? \"x\" \"y\")",             "#f",
            "(eqv? #\\a #\\a)",               "#t"
        };

        printf("Testing 'eqv?' (primitive)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] = 
        {
            "(eq? 'x 'x)",                   "#t",
            "(eq? 'x 'y)",                   "#f",
            "(eq? 5 5)",                     "#t",
            "(eq? 5 4)",                     "#f",
            "(eq? 5 5.0)",                   "#f",
            "(eq? \"x\" \"x\")",             "#f",
            "(eq? \"x\" \"y\")",             "#f",
            "(eq? #\\a #\\a)",               "#t"
        };

        printf("Testing 'eq?' (primitive)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] = 
        {
            "(equal? (list 1) (list 1))",       "#t",
            "(equal? (list 1) (list 2))",       "#f",
            "(equal? '(1 2 3) '(1 2 3))",       "#t",
            "(equal? '(1 2 3) '(1 2 4))",       "#f",
            "(equal? '(1 2 3) '(1 2))",         "#f"
        };

        printf("Testing 'equal?' (list)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] = 
        {
            "(eqv? '() '())",               "#t",
            "(eqv? '(1) '(1))",             "#f",
            "(eq? '() '())",                "#t",
            "(eq? '(1) '(1))",              "#f"
        };

        printf("Testing 'eqv? / eq?' (list)\n");
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

        printf("Testing list construction\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "'()",                  "'()",
            "'(1)",                 "'(1)",
            "'(1 2)",               "'(1 2)",
            "'(1 2 3 4)",           "'(1 2 3 4)"
        };

        printf("Testing list construction (shorthand)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 7;
        char strs[14][256] =
        {
            "(list? 2)",                "#f",
            "(list? '())",              "#t",
            "(list? '(1))",             "#t",
            "(list? '(1 2))",           "#t",
            "(list? '(1 . 2))",         "#f",
            "(list? '(1 2 3))",         "#t",
            "(list? '(1 2 . 3))",       "#f"
        };

        printf("Testing list?\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(length (list))",              "0",
            "(length (list 1))",            "1",
            "(length (list 1 2))",          "2",
            "(list-ref (list 1) 0)",        "1",
            "(list-ref (list 1 2) 0)",      "1",
            "(list-ref (list 1 2) 1)",      "2",
        };

        printf("Testing list accessors\n");
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

        printf("Testing pairs\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "'(1 . 2)",                     "'(1 . 2)",
            "'(1 . (2 . 3))",               "'(1 2 . 3)",
            "'(1 . (2 . (3 . 4)))",         "'(1 2 3 . 4)",
            "'(1 . ())",                    "'(1)",
            "'(1 . (1 2))",                 "'(1 1 2)",
            "'(1 2 . 3)",                   "'(1 2 . 3)",
            
            "(1 . + . 2)",                  "3",
            "(1 . + . (2 . + . 3))",        "6"
        };

        printf("Testing pairs (shorthand)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(car '(1 . 2))",                       "1",
            "(car '(1))",                           "1",
            "(car (list 1 2 3))",                   "1",
            "(cdr '(1 . 2))",                       "2",
            "(cdr '(1))",                           "'()",
            "(cdr (list 1 2 3))",                   "'(2 3)"
        };

        printf("Testing 'car' and 'cdr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(caar '((1) . 2))",                   "1",
            "(caar '((1 . 2) . 3))",                "1",
            "(cdar '((1) . 2))",                    "'()",
            "(cdar '((1 . 2) . 3))",                "2",
            "(cadr '(1 . (2)))",                    "2",
            "(cadr '(1 . (2 . 3)))",                "2",
            "(cddr '(1 . (2)))",                    "'()",
            "(cddr '(1 . (2 . 3)))",                "3"
        };

        printf("Testing 'caar', 'cdar', 'cadr', 'cddr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(caaar '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "1",
            "(caadr '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "5",
            "(cadar '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "3",
            "(caddr '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "7",
            "(cdaar '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "2",
            "(cdadr '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "6",
            "(cddar '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "4",
            "(cdddr '(((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))))",     "8"
        };

        printf("Testing 'caaar', ... 'cdddr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 16;
        char strs[32][256] =
        {
            "(caaaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "1",
            "(caaadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "9",
            "(caadar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "5",
            "(caaddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "13",
            "(cadaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "3",
            "(cadadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "11",
            "(caddar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "7",
            "(cadddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "15",
            "(cdaaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "2",
            "(cdaadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "10",
            "(cdadar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "6",
            "(cdaddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "14",
            "(cddaar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "4",
            "(cddadr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "12",
            "(cdddar '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "8",
            "(cddddr '((((1 . 2) . (3 . 4)) . ((5 . 6) . (7 . 8))) . (((9 . 10) . (11 . 12)) . ((13 . 14) . (15 . 16)))))",     "16"
        };

        printf("Testing 'caaaar', ... 'cddddr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(apply list (list))",                  "'()",
            "(apply + (list 1))",                   "1",
            "(apply + (list 1 2 3 4))",             "10",
            "(apply + 1 2 (list 3 4))",             "10",
            "(apply + 1 2 3 (list 4 5))",           "15"
        };

        printf("Testing 'apply'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(lambda () 1)",                                "<function>",
            "(lambda (x) (+ x 1))",                         "<function>",
            "(lambda (x y) (+ (* x x) (* y y)))",           "<function>"
        };

        printf("Testing 'lambda'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(map (lambda (x) (+ x 1)) (list))",            "'()",
            "(map (lambda (x) (+ x 1)) (list 1))",          "'(2)",
            "(map (lambda (x) (+ x 1)) (list 1 2 3))",      "'(2 3 4)",
            "(map + (list 1 2 3) (list 2 4 6))",            "'(3 6 9)"
        };

        printf("Testing 'map'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(andmap positive? (list 1 2 3))",      "#t",
            "(andmap positive? (list 1 -2 3))",     "#f",
            "(andmap positive? (list -1 -2 -3))",   "#f"
        };

        printf("Testing 'andmap'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(ormap positive? (list 1 2 3))",      "#t",
            "(ormap positive? (list 1 -2 3))",     "#t",
            "(ormap positive? (list -1 -2 -3))",   "#f"
        };

        printf("Testing 'ormap'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(append (list))",                              "'()",
            "(append (list) (list))",                       "'()",
            "(append (list) (list 1 2))",                   "'(1 2)",
            "(append (list 1 2) (list))",                   "'(1 2)",
            "(append (list 1 2) (list 3 4))",               "'(1 2 3 4)",
            "(append (list 1 2) (list 3 4) (list 5 6))",    "'(1 2 3 4 5 6)"
        };

        printf("Testing 'append'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(reverse (list))",                         "'()",
            "(reverse (list 1))",                       "'(1)",
            "(reverse (list 1 2 3 4 5 6))",             "'(6 5 4 3 2 1)"
        };

        printf("Testing 'reverse'..\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 10;
        char strs[20][256] =
        {
            "(let-values () 1)",                                            "1",
            "(let-values ([(x) 1]) x)",                                     "1",
            "(let-values ([(x) 1]) (+ x 1))",                               "2",
            "(let-values ([(x y) (values 1 2)]) (+ x y))",                  "3",
            "(let-values ([(x y) (values 1 2)] [(z) 3]) (+ x y z))",        "6",
            "(let*-values () 1)",                                           "1",
            "(let*-values ([(x) 1]) x)",                                    "1",
            "(let*-values ([(x) 1]) (+ x 1))",                              "2",
            "(let*-values ([(x y) (values 1 2)]) (+ x y))",                 "3",
            "(let*-values ([(x y) (values 1 2)] [(z) x]) (+ x y z))",       "4"             
        };

        printf("Testing 'let-values/let-values*'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] = 
        {
            "(begin 1)",                            "1",
            "(begin (def-values (x) 1) x)",                  "1",
            "(begin (def-values (x) 1) (def-values (y) 1) (+ x y))",  "2"  
        };

        printf("Testing 'begin'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }
    
    {
        const int COUNT = 4;
        char strs[8][256] = 
        {
            "(equal? + +)",             "#t",
            "(equal? + -)",             "#f",

            "(begin (def-values (ident) (lambda (x) x)) (equal? ident ident))",
            "#t",
            
            "(begin (def-values (f g) (values (lambda (x) x) (lambda (x) x))) (equal? f g))",
            "#f"
        };

        printf("Testing 'equal?' (functions)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(call/cc (lambda (e) 1))",         "1",
            "(call/cc (lambda (e) (e) 1))",     "<void>",
            "(call/cc (lambda (e) (e 2) 1))",   "2"
        };

        printf("Testing 'call/cc'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(vector)",             "'#()",
            "(vector 1)",           "'#(1)",
            "(vector 1 2 3)",       "'#(1 2 3)"
        };

        printf("Testing 'vector'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(make-vector 0)",      "'#()",
            "(make-vector 1)",      "'#(0)",
            "(make-vector 3)",      "'#(0 0 0)",
            "(make-vector 0 1)",    "'#()",
            "(make-vector 1 1)",    "'#(1)",
            "(make-vector 3 1)",    "'#(1 1 1)"
        };

        printf("Testing 'make-vector'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(vector-length (vector))",         "0",
            "(vector-length (vector 1))",       "1",
            "(vector-length (vector 1 2 3))",   "3"
        };

        printf("Testing 'vector-length'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(vector-ref (vector 1) 0)",        "1",
            "(vector-ref (vector 1 2 3) 1)",    "2",
            "(vector-ref (vector 1 2 3) 2)",    "3"
        };

        printf("Testing 'vector-ref'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(vector->list (vector))",          "'()",
            "(vector->list (vector 1))",        "'(1)",
            "(vector->list (vector 1 2 3))",    "'(1 2 3)"
        };

        printf("Testing 'vector->list'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(list->vector '())",         "'#()",
            "(list->vector '(1))",        "'#(1)",
            "(list->vector '(1 2 3))",    "'#(1 2 3)"
        };

        printf("Testing 'list->vector'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] = 
        {
            "(equal? (vector) (vector))",           "#t",
            "(equal? (vector 1) (vector 2))",       "#f",
            "(equal? #(1 2 3) #(1 2 3))",           "#t",
            "(equal? #(1 2) #(1 2 3))",             "#f",
            "(equal? #(1 2 3 4) #(1 2 3 5))",       "#f"
        };

        printf("Testing 'equal?' (vector)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] = 
        {
            "(eqv? #() #())",               "#t",
            "(eqv? #(1) #(1))",             "#f",
            "(eq? #() #())",                "#t",
            "(eq? #(1) #(1))",              "#f"
        };

        printf("Testing 'eqv? / eq?' (vector)\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 2;
        char strs[2][256] =
        {
            "(error \"this is an error\")",
            "(error 'foo \"this is also an error\")"
        };

        printf("Testing 'error'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= evaluate(strs[i]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(procedure? 1)",               "#f",
            "(procedure? (list 1))",        "#f",
            "(procedure? +)",               "#t",
            "(procedure? (lambda (x) x))",  "#t"
        };

        printf("Testing 'procedure?'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(procedure-arity exp)",                "1",
            "(procedure-arity +)",                  "'(0 . #f)",
            "(procedure-arity make-string)",        "'(1 . 2)",
            "(procedure-arity (lambda (x) x))",     "1",
            "(procedure-arity (lambda x x))",       "'(0 . #f)"
        };

        printf("Testing 'procedure-arity'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(delay 1)",                        "<promise:1>",
            "(delay (list 1 2 3))",             "<promise:(list 1 2 3)>",
            "(force (delay 1))",                "1",
            "(force (delay (list 1 2 3)))",     "'(1 2 3)"
        };

        printf("Testing 'delay/force'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "`1",                           "1",
            "`(1 2 3)",                     "'(1 2 3)",
            "`(1 null)",                    "'(1 null)",
            "`(1 ,null)",                   "'(1 ())",
            "`(1 2 ,(+ 1 2 3))",            "'(1 2 6)",
            "(begin (def-values (a) 1) `(1 ,a))",    "'(1 1)"
        };

        printf("Testing quasiquote / unquote\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(current-input-port)",                     "<port>",
            "(current-output-port)",                    "<port>",
            "(input-port? (current-input-port))",       "#t",
            "(output-port? (current-output-port))",     "#t",
            "(eof? eof)",                               "#t"
        };

        printf("Testing port procedures\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

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
        const int COUNT = 6;
        char strs[12][256] = 
        {
            "(equal? 'x 'x)",                   "#t",
            "(equal? 'x 'y)",                   "#f",
            "(equal? 5 5)",                     "#t",
            "(equal? 5 4)",                     "#f",
            "(equal? \"x\" \"x\")",             "#t",
            "(equal? \"x\" \"y\")",             "#f"
        };

        printf("Testing 'equal?' (primitive)\n");
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
            "(lambda () 1)",                                "<function:?>",
            "(lambda (x) (+ x 1))",                         "<function:?>",
            "(lambda (x y) (+ (* x x) (* y y)))",           "<function:?>"
        };

        printf("Testing 'lambda'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(map (lambda (x) (+ x 1)) (list))",            "'()",
            "(map (lambda (x) (+ x 1)) (list 1))",          "'(2)",
            "(map (lambda (x) (+ x 1)) (list 1 2 3))",      "'(2 3 4)"
        };

        printf("Testing 'map'\n");
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
        const int COUNT = 7;
        char strs[14][256] =
        {
            "(remove 0 (list))",                        "'()",
            "(remove 1 (list 1))",                      "'()",
            "(remove 1 (list 1 2 3))",                  "'(2 3)",
            "(remove 2 (list 1 2 3))",                  "'(1 3)",
            "(remove 4 (list 1 2 3))",                  "'(1 2 3)",
            "(remove 2 (list 1 2 3 2 1))",              "'(1 3 1)",
            "(remove 1 (list 1 2 3 2 1))",              "'(2 3 2)"
        };

        printf("Testing 'remove'..\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "(member 0 (list))",                        "#f",
            "(member 1 (list 1))",                      "'(1)",
            "(member 2 (list 1 2 3))",                  "'(2 3)",
            "(member (list 1 2) (list (list 1 2) 3))",  "'((1 2) 3)"
        };

        printf("Testing 'member'..\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(filter negative? (list))",                    "'()",
            "(filter negative? (list 1))",                  "'()",
            "(filter negative? (list -1))",                 "'(-1)",
            "(filter negative? (list -1 0 1))",             "'(-1)",
            "(filter negative? (list -3 -2 -1 0))",         "'(-3 -2 -1)"
        };

        printf("Testing 'filter'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 5;
        char strs[10][256] =
        {
            "(filtern negative? (list))",                    "'()",
            "(filtern negative? (list 1))",                  "'(1)",
            "(filtern negative? (list -1))",                 "'()",
            "(filtern negative? (list -1 0 1))",             "'(0 1)",
            "(filtern negative? (list -1 0 2 3))",           "'(0 2 3)"
        };

        printf("Testing 'filtern'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }


    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(foldl + 0 (list))",                   "0",
            "(foldl + 0 (list 1))",                 "1",
            "(foldl + 0 (list 1 2 3))",             "6",
            "(foldl cons (list) (list))",           "'()",
            "(foldl cons (list) (list 1))",         "'(1)",
            "(foldl cons (list) (list 1 2 3))",     "'(3 2 1)",

            "(begin (def-values (foo) (lambda (x y) x)) (foldl foo 0 (list 1)))",        "1",
            "(begin (def-values (foo) (lambda (x y) x)) (foldl foo 0 (list 1 2 3)))",    "3"
        };

        printf("Testing 'foldl'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(foldr + 0 (list))",                   "0",
            "(foldr + 0 (list 1))",                 "1",
            "(foldr + 0 (list 1 2 3))",             "6",
            "(foldr cons (list) (list))",           "'()",
            "(foldr cons (list) (list 1))",         "'(1)",
            "(foldr cons (list) (list 1 2 3))",     "'(1 2 3)",

            "(begin (def-values (foo) (lambda (x y) x)) (foldr foo 0 (list 1)))",        "1",
            "(begin (def-values (foo) (lambda (x y) x)) (foldr foo 0 (list 1 2 3)))",    "1"
        };

        printf("Testing 'foldr'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 12;
        char strs[24][256] =
        {
            "(let () 1)",                       "1",
            "(let ((x 1)) x)",                  "1",
            "(let ((x 1)) (+ x 1))",            "2",
            "(let ((x 1) (y 2)) (+ x y))",      "3",
            "(let () 1 2)",                     "2",
            "(let* () 1)",                      "1",
            "(let* ((x 1)) x)",                 "1",
            "(let* ((x 1)) (+ x 1))",           "2",
            "(let* ((x 1) (y 2)) (+ x y))",     "3",  
            "(let* ((x 1) (y x)) y)",           "1",
            "(let* ((x 1) (y x)) (+ x y))",     "2",
            "(let* () 1 2)",                    "2"                 
        };

        printf("Testing 'let/let*'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 8;
        char strs[16][256] =
        {
            "(let f () 1)",                     "1",
            "(let f ((x 2)) x)",                "2",
            "(let f ((x 1) (y 2)) (+ x y))",    "3",
            "(let* f () 1)",                    "1",
            "(let* f ((x 2)) x)",               "2",
            "(let* f ((x 1) (y x)) (+ x y))",   "2",

            "(let fact ([n 4]) (if (zero? n) 1 (* n (fact (- n 1)))))",     "24",
            "(let foo ([n 4]) (set! n 2) n)",                               "2"
        };

        printf("Testing 'named let/let*'\n");
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
        const int COUNT = 5;
        char strs[10][256] = 
        {   
            "(case)",                                   "<void>",
            "(case 5 [(5) #t])",                        "#t",
            "(case 5 [(4) #t])",                        "<void>",
            "(case 5 [(1 3 5) 'odd] [(2 4 6) 'even])",  "'odd",
            "(case 4 [(1 3 5) 'odd] [(2 4 6) 'even])",  "'even"
        };

        printf("Testing 'case'\n");
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
            "(procedure-arity error)",           "'(1 . 2)",
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

    GC_finalize();

    return (int)(!status);
}

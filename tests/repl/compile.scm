; The compiler, %compile-form: the core to the simplified core.
(import (minim system))

; Each core form's simplified form.
(define forms
  '(x
    5
    #(1 2)
    (quote (a b))
    (if a b)
    (if a b c)
    (begin)
    (begin a)
    (begin a b c)
    (define x 1)
    (set! x (f 1))
    (define-values (a . b) (f))
    (let-values (((a) 1) ((b . c) (f))) a b)
    (letrec-values (((loop) (lambda (n) (loop n)))) (loop 0))
    (lambda (x) x)
    (lambda (x y . z) (f x) z)
    (lambda args args)
    (case-lambda ((x) x) ((x y) y) (rest rest))
    (case-lambda)
    (f x 1)
    ((lambda (x) x) 1)
    (begin (define a 1) (define b (lambda () a)))))
(for-each (lambda (x) (write (%compile-form x)) (newline)) forms)

; Each pass's output is in its language; the checkers reject the others.
(map (lambda (x) (list (%parsed? (%parse x #t)) (%simplified? (%compile-form x)))) forms)
(list (%simplified? '(lambda ((x) x))) (%parsed? '(clambda ((x) 1 #f x)))
      (%simplified? '(clambda ((x) 2 #f x))) (%simplified? '(if a b))
      (%simplified? '(begin a)) (%simplified? '(f x)) (%simplified? 5))

; The values the compiled forms give are the evaluator's.
(define programs
  '((letrec-values (((fact) (lambda (n) (if (= n 0) 1 (* n (fact (- n 1)))))))
      (fact 10))
    (let-values (((a b) (values 1 2)) ((c . d) (values 3 4 5)))
      (set! a (+ a 10))
      (list a b c d))
    ((case-lambda ((x) (list 'one x)) ((x . y) (list 'more x y))) 1 2 3)
    ((lambda args args))
    (if #f #f)
    (begin (define-values (compiled-a . compiled-b) (values 1 2)) (list compiled-a compiled-b))))
(map (lambda (x) (equal? (%eval-expanded x) (%eval-compiled (%compile-form x)))) programs)
(map (lambda (x) (%eval-compiled (%compile-form x))) programs)

; The evaluator's bad syntax, raised by the compiler with the same irritant.
(define (failure thunk)
  (guard (e (#t (list (error-object-message e) (error-object-irritants e))))
    (thunk)
    'no-error))
(define malformed
  '(()
    (quote)
    (quote a b)
    (if a)
    (if a b c d)
    (define x)
    (define 1 2)
    (set! x)
    (set! (x) 1)
    (define-values (1) 2)
    (let-values (((1) 2)) 3)
    (let-values ())
    (let-values (x) 1)
    (lambda (x))
    (lambda (1) x)
    (case-lambda (x))
    (f . x)
    (begin a . b)))
(map (lambda (x)
       (equal? (failure (lambda () (%eval-expanded x)))
               (failure (lambda () (%compile-form x)))))
     malformed)

; Definitions only at top level, or in a top-level begin.
(failure (lambda () (%compile-form '(lambda () (define x 1)))))
(failure (lambda () (%compile-form '(let-values () (define-values (x) 1) x))))
(failure (lambda () (%compile-form '(if a (define x 1)))))
(failure (lambda () (%compile-form '(begin (define x 1) (begin (define y 2))))))

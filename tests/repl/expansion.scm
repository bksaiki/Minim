; The internals this file tests are in (minim system).
(import (minim system))

; What forms expand to.  Fresh names print as the names they replace.

; Bodies, with and without definitions.
(%expand '(lambda (a) (+ a 1)))
(%expand '(lambda (a) (define x 1) (+ a x)))
(%expand '(lambda () (define-values (x y) (values 1 2)) (define z 3) (+ x y z)))
(%expand '(lambda () (define-syntax m (syntax-rules () ((_) 1))) (m)))
(%expand '(lambda () (define f (lambda () (define g 1) g)) (f)))
(%expand '(lambda () (define x 1)))
(%expand '(define (f a . r) (cons a r)))
(%expand '(lambda () (define (g) 1) (g)))
(%expand '(case-lambda ((x) x) ((x . y) (define z y) z) (() 0)))

; %eval-expanded runs a core form globally, unexpanded.
(%eval-expanded '(+ 1 2))
(%eval-expanded '(values 1 2))
(%eval-expanded '(define evaluated 5))
evaluated
(%eval-expanded '((lambda (x) (if x 'yes 'no)) #t))
(%eval-expanded '(let-values (((a b) (values 1 2))) (+ a b)))
(define (count-down n)
  (if (= n 0) 'done (%eval-expanded (list count-down (- n 1)))))
(count-down 100000)
(%eval-expanded '(when #t 1))
(%eval-expanded '(if))
(apply %eval-expanded '((+ 1 2)))

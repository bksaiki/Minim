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

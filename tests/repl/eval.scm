; eval in an environment of imports (R7RS 6.12).
(define base (environment '(scheme base)))
base
(eval '(+ 1 2) base)
(eval '(let ((x 2)) (* x x)) base)
(eval '(call-with-values (lambda () (values 1 2)) list) base)
(eval '(display "hi") base)
(eval '(begin (display "hi") (newline)) (environment '(scheme base) '(scheme write)))
(eval '(gensym) (environment '(only (scheme base) car)))

; Its bindings are immutable, and the same sets give the same environment.
(eval '(define x 1) base)
(eval '(set! car 1) base)
(eq? base (environment '(scheme base)))
(eq? base (environment '(scheme write)))

; The interaction environment is the REPL's.
(eval '(define from-eval 7) (interaction-environment))
from-eval
(eval '(define-syntax swap (syntax-rules () ((_ a b) (list b a)))) (interaction-environment))
(swap 1 2)
(eval '(import (prefix (scheme base) b:)) (interaction-environment))
(b:car '(1))
(eq? (interaction-environment) (interaction-environment))

; Errors.
(eval '(car '()) base)
(eval '(if) base)
(eval 1 'base)
(environment '(no such library))

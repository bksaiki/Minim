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

; Mutating the import sets passed doesn't change the cache.
(define sets (list (list 'scheme 'file)))
(define file-env (apply environment sets))
(set-car! (car sets) 'nope)
(eq? file-env (environment '(nope file)))
(eq? file-env (environment '(scheme file)))

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

; R5RS's environments.
(eval '(car '(1)) (scheme-report-environment 5))
(eval '(let ((x 1)) (if x 'yes 'no)) (null-environment 5))
(eval '(cond (#f 1) (else 'none)) (null-environment 5))
(eval '(car '(1)) (null-environment 5))
(eq? (null-environment 5) (null-environment 5))
(scheme-report-environment 4)
(null-environment 7)

; interaction-environment is a parameter: the REPL, eval, and load follow it.
(define repl (interaction-environment))
(parameterize ((interaction-environment base))
  (list (eq? (interaction-environment) base) (eval '(+ 1 2) (interaction-environment))))
(eq? (interaction-environment) repl)
(load "load/minim-name.scm")
(parameterize ((interaction-environment base))
  (load "load/minim-name.scm"))
(parameterize ((interaction-environment base))
  (eval '(define y 1) (interaction-environment)))
(interaction-environment 5)

; Set at the REPL to an immutable environment, it takes only expressions, and
; nothing there can set it back: the end of this file.
(interaction-environment base)
(define y 1)
(+ 1 2)
(gensym)
(interaction-environment repl)

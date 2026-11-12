; The compile hook, %compile: a parameter holding the compiler, or #f.  Every
; top-level form, eval's included, is compiled while it holds one.
(import (minim system))
(eq? (%compile) %compile-form)
(car (%closure-code (lambda (x) x)))
(car (%closure-code (eval '(lambda (x) x) (interaction-environment))))
(car (%closure-code (parameterize ((%compile #f))
                      (eval '(lambda (x) x) (interaction-environment)))))
(car (%closure-code (lambda (x) x)))
(%compile #f)
(car (%closure-code (lambda (x) x)))
(%compile %compile-form)
(car (%closure-code (lambda (x) x)))

; A core form that doesn't compile raises before any of it runs.
(define ran #f)
(%eval-expanded '(begin (set! ran #t) (if)))
ran

; The internals this file tests are in (minim system).
(import (minim system))

; Parameters (R7RS 4.2.6).
(define p (make-parameter 1))
(p)
(procedure? p)
(p 2)
(p)

; The converter applies to the initial value, once, and to each value set.
(define calls 0)
(define q
  (make-parameter 10 (lambda (x) (set! calls (+ calls 1)) (* x 2))))
(q)
calls
(q 5)
(q)
calls
(make-parameter 1 (lambda (x) (error "bad value" x)))
(p 1 2)

; parameter?, in (minim), tells parameters apart by their code, without
; calling them.
(parameter? p)
(parameter? (lambda () 1))
(parameter? car)
(parameter? 1)
(eq? (%closure-code p) (%closure-code q))
(%closure-code (lambda (x) x))
(%closure-code car)

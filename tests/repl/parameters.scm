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

; parameterize binds for the body's dynamic extent.
(define a (make-parameter 'a0))
(define b (make-parameter 'b0))
(parameterize ((a 'a1) (b 'b1)) (list (a) (b)))
(list (a) (b))
(parameterize ((a 'a1)) (parameterize ((a 'a2)) (a)))
(parameterize ((a 'a1)) (parameterize ((a 'a2)) 'inner) (a))
(parameterize ((a 1) (a 2)) (a))
(a)
(parameterize () 1)
(parameterize ((a 'a1))
  (define x (a))
  (list x 'defined))

; The converter applies to the new value, but not to the restored one.
(set! calls 0)
(parameterize ((q 1)) (q))
(q)
calls

; Leaving by an escape restores the value.
(call/cc (lambda (k) (parameterize ((a 'escaped)) (k (a)))))
(a)
(guard (e (#t (list e (a)))) (parameterize ((a 'raised)) (raise (a))))
(a)

; Re-entering the body binds the value again; one set inside survives.
(define k #f)
(define s (make-parameter 0))
(parameterize ((s 1))
  (call/cc (lambda (c) (set! k c)))
  (s (+ (s) 1))
  (s))
(s)
(if k (let ((c k)) (set! k #f) (c #f)) 'done)
(s)

; A non-parameter is an error, and the body doesn't run.
(parameterize ((car 1)) (display "ran"))
(parameterize ((a 1) ('x 2)) 'never)

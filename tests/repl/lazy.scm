; delay evaluates its expression once, when first forced.
(define p (delay (begin (display "once ") 42)))
p
(promise? p)
(list (force p) (force p))
(promise? p)

; make-promise: a forced promise of a value, or the promise itself.
(force (make-promise 5))
(promise? (make-promise 5))
(let ((q (delay 1))) (eq? q (make-promise q)))
(force (make-promise (make-promise 'x)))
(map promise? (list 1 'a (lambda () 1)))

; A reentrant force (R7RS 4.2.5): the first value wins.
(define count 0)
(define x 5)
(define r (delay (begin (set! count (+ count 1)) (if (> count x) count (force r)))))
(force r)
(begin (set! x 10) (force r))

; A chain of delay-forces collapses into one promise as it is forced.
(define (chain n) (if (= n 0) (delay 'done) (delay-force (chain (- n 1)))))
(force (chain 1000))

; A promise that raises stays unforced, so forcing it again retries.
(define tries 0)
(define flaky (delay (begin (set! tries (+ tries 1)) (if (= tries 1) (error "first try") 'ok))))
(guard (e (#t (error-object-message e))) (force flaky))
(force flaky)
tries

; Errors
(force 5)
(force (delay-force 5))

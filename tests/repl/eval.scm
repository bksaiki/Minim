(+ 1 2)
(define x 10)
x
(define (square n) (* n n))
(square 12)
(define (make-counter)
  (define n 0)
  (lambda () (set! n (+ n 1)) n))
(define c (make-counter))
(c)
(c)
; An evaluation error is reported on stderr and the loop continues.
(car 5)
(cdr '(1 2))

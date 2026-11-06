; The values vary, so these check their properties.

; current-second: an inexact real, after the start of 2025.
(define s (current-second))
(list (real? s) (inexact? s) (> s 1735689600))

; current-jiffy: an exact integer that never decreases.
(exact-integer? (current-jiffy))
(let loop ((i 0) (prev (current-jiffy)))
  (or (= i 1000)
      (let ((j (current-jiffy)))
        (and (>= j prev) (loop (+ i 1) j)))))

; jiffies-per-second: an exact positive integer.
(list (exact-integer? (jiffies-per-second)) (positive? (jiffies-per-second)))

; A busy loop takes a positive number of jiffies.
(define start (current-jiffy))
(let loop ((i 0))
  (when (< i 10000)
    (loop (+ i 1))))
(> (current-jiffy) start)

; (scheme time) in a library.
(define-library (timer)
  (export elapsed)
  (import (scheme base) (scheme time))
  (begin
    (define (elapsed thunk)
      (let ((t (current-jiffy)))
        (thunk)
        (- (current-jiffy) t)))))
(import (timer))
(exact-integer? (elapsed (lambda () 'done)))

; They take no arguments.
(current-second 1)

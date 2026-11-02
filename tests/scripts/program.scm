(import (scheme base) (scheme write))

(define (square x)
  (* x x))

(display (map square '(1 2 3)))
(newline)

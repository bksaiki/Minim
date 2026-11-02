; Loaded by libraries.scm; imports another library file.
(define-library (lib shapes)
  (export distance-squared)
  (import (scheme base) (lib point))
  (begin
    (define (square x)
      (* x x))

    (define (distance-squared a b)
      (+ (square (- (point-x b) (point-x a)))
         (square (- (point-y b) (point-y a)))))))

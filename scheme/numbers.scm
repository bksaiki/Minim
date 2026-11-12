;;; Numbers (R7RS 6.2.6): the tests and procedures built on arithmetic and
;;; comparison, integer division, and R5RS's names for `inexact` and `exact`.

(define (zero? z)
  (%check number? z "zero?: not a number")
  (= z 0))

(define (positive? x)
  (%check number? x "positive?: not a number")
  (> x 0))

(define (negative? x)
  (%check number? x "negative?: not a number")
  (< x 0))

(define (nan? z)
  (%check number? z "nan?: not a number")
  (not (= z z)))

(define (infinite? z)
  (%check number? z "infinite?: not a number")
  (or (= z +inf.0) (= z -inf.0)))

(define (finite? z)
  (%check number? z "finite?: not a number")
  (not (or (infinite? z) (nan? z))))

;; Whether the integer `n` is odd; raises `msg` unless it is an integer.  An
;; inexact `n` is odd if `n / 2` isn't integral, which holds past 2^53, where
;; every flonum is even.
(define (%odd? n msg)
  (unless (integer? n)
    (error msg n))
  (if (fixnum? n)
      (not (= (remainder n 2) 0))
      (not (integer? (/ n 2)))))

(define (odd? n)
  (%odd? n "odd?: not an integer"))

(define (even? n)
  (not (%odd? n "even?: not an integer")))

;; The least fixnum, whose negation isn't one.
(define %fixnum-min -1152921504606846976)

(define (abs x)
  (%check number? x "abs: not a number")
  (cond ((eqv? x %fixnum-min) (error "abs: fixnum overflow"))
        ((or (< x 0) (eqv? x -0.0)) (- x))
        (else x)))

;; The greatest of `x` and `xs` if `greater?` is `>`, or the least if it is
;; `<`: inexact if any is, and NaN if any is.  Raises `msg` on a non-number.
(define (%extreme greater? msg x xs)
  (%check number? x msg)
  (let loop ((r x) (xs xs) (any-inexact (inexact? x)))
    (if (null? xs)
        (if any-inexact (inexact r) r)
        (let ((y (car xs)))
          (%check number? y msg)
          (loop (cond ((or (nan? r) (nan? y)) +nan.0)
                      ((greater? y r) y)
                      (else r))
                (cdr xs)
                (or any-inexact (inexact? y)))))))

(define (max x . xs)
  (%extreme > "max: not a number" x xs))

(define (min x . xs)
  (%extreme < "min: not a number" x xs))

(define exact->inexact (procedure-rename inexact 'exact->inexact))

(define inexact->exact (procedure-rename exact 'inexact->exact))

;;; Integer division, over `quotient` and `remainder`, which truncate.

(define truncate-quotient (procedure-rename quotient 'truncate-quotient))

(define truncate-remainder (procedure-rename remainder 'truncate-remainder))

;; Whether `r`, the remainder of a truncating division by `m`, has the other
;; sign, so flooring adjusts the result.
(define (%floor-adjusts? r m)
  (not (or (zero? r) (eq? (negative? r) (negative? m)))))

(define (floor-quotient n m)
  (let ((q (quotient n m)))
    (if (%floor-adjusts? (remainder n m) m) (- q 1) q)))

(define (floor-remainder n m)
  (let ((r (remainder n m)))
    (if (%floor-adjusts? r m) (+ r m) r)))

(define modulo (procedure-rename floor-remainder 'modulo))

(define (floor/ n m)
  (values (floor-quotient n m) (floor-remainder n m)))

(define (truncate/ n m)
  (values (quotient n m) (remainder n m)))

;; `op` folded over the integers `ns` from `init`; inexact if any is.  Raises
;; `msg` on a non-integer.
(define (%fold-integers op init msg ns)
  (let loop ((ns ns) (acc init) (any-inexact #f))
    (if (null? ns)
        (if any-inexact (inexact acc) acc)
        (let ((n (car ns)))
          (%check integer? n msg)
          (loop (cdr ns) (op acc n) (or any-inexact (inexact? n)))))))

;; The greatest common divisor of the integers `a` and `b`, by Euclid's.
(define (%gcd a b)
  (if (zero? b)
      (abs a)
      (%gcd b (remainder a b))))

(define (gcd . ns)
  (%fold-integers %gcd 0 "gcd: not an integer" ns))

(define (lcm . ns)
  (%fold-integers (lambda (a b)
                    (if (or (zero? a) (zero? b)) 0 (abs (* (quotient a (%gcd a b)) b))))
                  1
                  "lcm: not an integer"
                  ns))

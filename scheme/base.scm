;;; `identity`, and the comparisons of booleans, symbols, and chars ignoring
;;; case.

;; Whether `ok?` holds of the keys of each adjacent pair of `xs`, where `key`
;; gives an element's key.  Raises `msg` with the first element not
;; satisfying `type?`, checking every element.
(define (%compare type? key ok? msg xs)
  (let check ((ys xs))
    (unless (null? ys)
      (unless (type? (car ys))
        (error msg (car ys)))
      (check (cdr ys))))
  (let loop ((xs xs))
    (or (null? (cdr xs))
        (and (ok? (key (car xs)) (key (cadr xs)))
             (loop (cdr xs))))))

(define (identity x)
  x)

(define (boolean=? b . bs)
  (%compare boolean? identity eq? "boolean=?: not a boolean" (cons b bs)))

(define (symbol=? s . ss)
  (%compare symbol? identity eq? "symbol=?: not a symbol" (cons s ss)))

(define (%folded c)
  (char->integer (char-foldcase c)))

(define (char-ci=? c . cs)
  (%compare char? %folded = "char-ci=?: not a char" (cons c cs)))

(define (char-ci<? c . cs)
  (%compare char? %folded < "char-ci<?: not a char" (cons c cs)))

(define (char-ci>? c . cs)
  (%compare char? %folded > "char-ci>?: not a char" (cons c cs)))

(define (char-ci<=? c . cs)
  (%compare char? %folded <= "char-ci<=?: not a char" (cons c cs)))

(define (char-ci>=? c . cs)
  (%compare char? %folded >= "char-ci>=?: not a char" (cons c cs)))

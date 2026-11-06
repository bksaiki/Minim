;;; Strings (R7RS 6.7), over `make-string`, `string-ref`, and `string-set!`.

;; Raises `msg` with `s` unless it is a string.
(define (%check-string s msg)
  (unless (string? s)
    (error msg s)))

;; Raises `msg` with `s` unless it is a mutable string.
(define (%check-mutable s msg)
  (unless (mutable-string? s)
    (error msg s)))

;; Raises `msg` with `k` unless it is a position in a sequence of length
;; `len`: an exact integer from 0 to `len`.
(define (%check-position k len msg)
  (unless (and (fixnum? k) (<= 0 k len))
    (error msg k)))

;; Raises `msg` unless `start` and `end` are positions in a sequence of
;; length `len`, `start` no later than `end`.
(define (%check-range start end len msg)
  (%check-position start len msg)
  (%check-position end len msg)
  (when (> start end)
    (error msg start)))

;; The length of `s` if it is a string, else 0: the default end of a range,
;; leaving the full form to check `s`.
(define (%length-or-0 s)
  (if (string? s) (string-length s) 0))

;; Copies the chars of `from` from `start` to `end` into `to` at `at`, in an
;; order safe when the two overlap.
(define (%copy-chars! to at from start end)
  (if (<= at start)
      (do ((i start (+ i 1)))
          ((= i end))
        (string-set! to (+ at (- i start)) (string-ref from i)))
      (do ((i (- end 1) (- i 1)))
          ((< i start))
        (string-set! to (+ at (- i start)) (string-ref from i)))))

(define (string . cs)
  (for-each (lambda (c) (unless (char? c) (error "string: not a char" c))) cs)
  (list->string cs))

(define (string-append . ss)
  (for-each (lambda (s) (%check-string s "string-append: not a string")) ss)
  (let ((new (make-string (apply + (map string-length ss)))))
    (let loop ((ss ss) (at 0))
      (if (null? ss)
          new
          (let ((n (string-length (car ss))))
            (%copy-chars! new at (car ss) 0 n)
            (loop (cdr ss) (+ at n)))))))

;; A new string of the chars of `s` from `start` to `end`.
(define (%substring s start end type-msg index-msg)
  (%check-string s type-msg)
  (%check-range start end (string-length s) index-msg)
  (let ((new (make-string (- end start))))
    (%copy-chars! new 0 s start end)
    new))

(define (substring s start end)
  (%substring s start end "substring: not a string" "substring: bad index"))

(define string-copy
  (case-lambda
    ((s) (string-copy s 0))
    ((s start) (string-copy s start (%length-or-0 s)))
    ((s start end) (%substring s start end "string-copy: not a string" "string-copy: bad index"))))

(define string-copy!
  (case-lambda
    ((to at from) (string-copy! to at from 0))
    ((to at from start) (string-copy! to at from start (%length-or-0 from)))
    ((to at from start end)
     (%check-string to "string-copy!: not a string")
     (%check-mutable to "string-copy!: immutable string")
     (%check-position at (string-length to) "string-copy!: bad index")
     (%check-string from "string-copy!: not a string")
     (%check-range start end (string-length from) "string-copy!: bad index")
     (when (> (- end start) (- (string-length to) at))
       (error "string-copy!: bad index" at))
     (%copy-chars! to at from start end))))

(define string-fill!
  (case-lambda
    ((s c) (string-fill! s c 0))
    ((s c start) (string-fill! s c start (%length-or-0 s)))
    ((s c start end)
     (%check-string s "string-fill!: not a string")
     (%check-mutable s "string-fill!: immutable string")
     (unless (char? c)
       (error "string-fill!: not a char" c))
     (%check-range start end (string-length s) "string-fill!: bad index")
     (do ((i start (+ i 1)))
         ((= i end))
       (string-set! s i c)))))

;; Compares the strings `a` and `b` by char: -1, 0, or 1.
(define (%string-order a b)
  (let ((na (string-length a))
        (nb (string-length b)))
    (let loop ((i 0))
      (cond ((= i na) (if (= i nb) 0 -1))
            ((= i nb) 1)
            ((char<? (string-ref a i) (string-ref b i)) -1)
            ((char>? (string-ref a i) (string-ref b i)) 1)
            (else (loop (+ i 1)))))))

;; Whether `cmp` holds of the order of each adjacent pair of the strings
;; `ss` and 0, each string first mapped by `key`; raises `msg` on a non-string.
(define (%compare-strings key cmp msg ss)
  (%compare string? key (lambda (a b) (cmp (%string-order a b) 0)) msg ss))

(define (string=? s . ss)
  (%compare-strings identity = "string=?: not a string" (cons s ss)))

(define (string<? s . ss)
  (%compare-strings identity < "string<?: not a string" (cons s ss)))

(define (string>? s . ss)
  (%compare-strings identity > "string>?: not a string" (cons s ss)))

(define (string<=? s . ss)
  (%compare-strings identity <= "string<=?: not a string" (cons s ss)))

(define (string>=? s . ss)
  (%compare-strings identity >= "string>=?: not a string" (cons s ss)))

(define (string-ci=? s . ss)
  (%compare-strings string-foldcase = "string-ci=?: not a string" (cons s ss)))

(define (string-ci<? s . ss)
  (%compare-strings string-foldcase < "string-ci<?: not a string" (cons s ss)))

(define (string-ci>? s . ss)
  (%compare-strings string-foldcase > "string-ci>?: not a string" (cons s ss)))

(define (string-ci<=? s . ss)
  (%compare-strings string-foldcase <= "string-ci<=?: not a string" (cons s ss)))

(define (string-ci>=? s . ss)
  (%compare-strings string-foldcase >= "string-ci>=?: not a string" (cons s ss)))

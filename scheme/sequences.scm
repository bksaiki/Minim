;;; Strings, vectors, and bytevectors (R7RS 6.7 to 6.9), over their
;;; constructors, lengths, and element accessors; and the higher-order
;;; procedures of strings and vectors (R7RS 6.10).

;; Raises `msg` with `x` unless `(type? x)`.
(define (%check type? x msg)
  (unless (type? x)
    (error msg x)))

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

;; The length of `x` if `(type? x)`, else 0: the default end of a range,
;; leaving the full form to check `x`.
(define (%length-or-0 x type? length)
  (if (type? x) (length x) 0))

;; Copies the elements of `from` from `start` to `end` into `to` at `at`, read
;; by `ref` and written by `set`, in an order safe when the two overlap.
(define (%copy! ref set to at from start end)
  (let ((offset (- at start)))
    (if (<= at start)
        (do ((i start (+ i 1)))
            ((= i end))
          (set to (+ i offset) (ref from i)))
        (do ((i (- end 1) (- i 1)))
            ((< i start))
          (set to (+ i offset) (ref from i))))))

;; A new sequence, made by `make`, of the elements of `x` from `start` to
;; `end`.
(define (%subsequence make ref set x start end)
  (let ((new (make (- end start))))
    (%copy! ref set new 0 x start end)
    new))

;; A new sequence, made by `make`, of the elements of the sequences `xs`;
;; raises `msg` with the first not satisfying `type?`.
(define (%append type? length make ref set msg xs)
  (for-each (lambda (x) (%check type? x msg)) xs)
  (let ((new (make (apply + (map length xs)))))
    (let loop ((xs xs) (at 0))
      (if (null? xs)
          new
          (let ((n (length (car xs))))
            (%copy! ref set new at (car xs) 0 n)
            (loop (cdr xs) (+ at n)))))))

;; `(copy x start end)`, checked: `x` by `type?`, raising `type-msg`, and the
;; range, raising `index-msg`.
(define (%copy type? length make ref set type-msg index-msg x start end)
  (%check type? x type-msg)
  (%check-range start end (length x) index-msg)
  (%subsequence make ref set x start end))

;; `(copy! to at from start end)`, checked as `%copy`, and that the range
;; fits in `to` after `at`.
(define (%copy-to! type? length ref set type-msg index-msg to at from start end)
  (%check type? to type-msg)
  (%check-position at (length to) index-msg)
  (%check type? from type-msg)
  (%check-range start end (length from) index-msg)
  (when (> (- end start) (- (length to) at))
    (error index-msg at))
  (%copy! ref set to at from start end))

;; `(fill! x fill start end)`, checked as `%copy`.
(define (%fill! type? length set type-msg index-msg x fill start end)
  (%check type? x type-msg)
  (%check-range start end (length x) index-msg)
  (do ((i start (+ i 1)))
      ((= i end))
    (set x i fill)))

;;; Strings

(define (string . cs)
  (for-each (lambda (c) (%check char? c "string: not a char")) cs)
  (list->string cs))

(define (string-append . ss)
  (%append string? string-length make-string string-ref string-set!
           "string-append: not a string" ss))

(define (substring s start end)
  (%copy string? string-length make-string string-ref string-set!
         "substring: not a string" "substring: bad index" s start end))

(define string-copy
  (case-lambda
    ((s) (string-copy s 0))
    ((s start) (string-copy s start (%length-or-0 s string? string-length)))
    ((s start end)
     (%copy string? string-length make-string string-ref string-set!
            "string-copy: not a string" "string-copy: bad index" s start end))))

(define string-copy!
  (case-lambda
    ((to at from) (string-copy! to at from 0))
    ((to at from start)
     (string-copy! to at from start (%length-or-0 from string? string-length)))
    ((to at from start end)
     (%check string? to "string-copy!: not a string")
     (%check mutable-string? to "string-copy!: immutable string")
     (%copy-to! string? string-length string-ref string-set!
                "string-copy!: not a string" "string-copy!: bad index" to at from start end))))

(define string-fill!
  (case-lambda
    ((s c) (string-fill! s c 0))
    ((s c start) (string-fill! s c start (%length-or-0 s string? string-length)))
    ((s c start end)
     (%check string? s "string-fill!: not a string")
     (%check mutable-string? s "string-fill!: immutable string")
     (%check char? c "string-fill!: not a char")
     (%fill! string? string-length string-set!
             "string-fill!: not a string" "string-fill!: bad index" s c start end))))

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

;;; Vectors

(define (vector . xs)
  (list->vector xs))

(define vector-copy
  (case-lambda
    ((v) (vector-copy v 0))
    ((v start) (vector-copy v start (%length-or-0 v vector? vector-length)))
    ((v start end)
     (%copy vector? vector-length make-vector vector-ref vector-set!
            "vector-copy: not a vector" "vector-copy: bad index" v start end))))

(define vector-copy!
  (case-lambda
    ((to at from) (vector-copy! to at from 0))
    ((to at from start)
     (vector-copy! to at from start (%length-or-0 from vector? vector-length)))
    ((to at from start end)
     (%copy-to! vector? vector-length vector-ref vector-set!
                "vector-copy!: not a vector" "vector-copy!: bad index" to at from start end))))

(define vector-fill!
  (case-lambda
    ((v x) (vector-fill! v x 0))
    ((v x start) (vector-fill! v x start (%length-or-0 v vector? vector-length)))
    ((v x start end)
     (%fill! vector? vector-length vector-set!
             "vector-fill!: not a vector" "vector-fill!: bad index" v x start end))))

(define (vector-append . vs)
  (%append vector? vector-length make-vector vector-ref vector-set!
           "vector-append: not a vector" vs))

(define string->vector
  (case-lambda
    ((s) (string->vector s 0))
    ((s start) (string->vector s start (%length-or-0 s string? string-length)))
    ((s start end)
     (%copy string? string-length make-vector string-ref vector-set!
            "string->vector: not a string" "string->vector: bad index" s start end))))

(define vector->string
  (case-lambda
    ((v) (vector->string v 0))
    ((v start) (vector->string v start (%length-or-0 v vector? vector-length)))
    ((v start end)
     (%check vector? v "vector->string: not a vector")
     (%check-range start end (vector-length v) "vector->string: bad index")
     (do ((i start (+ i 1)))
         ((= i end))
       (%check char? (vector-ref v i) "vector->string: not a char"))
     (%subsequence make-string vector-ref string-set! v start end))))

;;; Bytevectors

(define (%byte? b)
  (and (fixnum? b) (<= 0 b 255)))

(define (bytevector . bs)
  (for-each (lambda (b) (%check %byte? b "bytevector: not a byte")) bs)
  (let ((bv (make-bytevector (length bs))))
    (do ((bs bs (cdr bs))
         (i 0 (+ i 1)))
        ((null? bs) bv)
      (bytevector-u8-set! bv i (car bs)))))

(define bytevector-copy
  (case-lambda
    ((bv) (bytevector-copy bv 0))
    ((bv start) (bytevector-copy bv start (%length-or-0 bv bytevector? bytevector-length)))
    ((bv start end)
     (%copy bytevector? bytevector-length make-bytevector bytevector-u8-ref bytevector-u8-set!
            "bytevector-copy: not a bytevector" "bytevector-copy: bad index" bv start end))))

(define bytevector-copy!
  (case-lambda
    ((to at from) (bytevector-copy! to at from 0))
    ((to at from start)
     (bytevector-copy! to at from start (%length-or-0 from bytevector? bytevector-length)))
    ((to at from start end)
     (%copy-to! bytevector? bytevector-length bytevector-u8-ref bytevector-u8-set!
                "bytevector-copy!: not a bytevector" "bytevector-copy!: bad index"
                to at from start end))))

(define (bytevector-append . bvs)
  (%append bytevector? bytevector-length make-bytevector bytevector-u8-ref bytevector-u8-set!
           "bytevector-append: not a bytevector" bvs))

;;; Higher-order procedures, over the elements as lists, to the shortest's
;;; length.

;; The string of `(proc c ...)` for the chars at each index of the strings.
(define (string-map proc s . strings)
  (list->string (apply map proc (map string->list (cons s strings)))))

;; Applies `proc` to the chars at each index of the strings, in order.
(define (string-for-each proc s . strings)
  (apply for-each proc (map string->list (cons s strings))))

;; The vector of `(proc x ...)` for the elements at each index of the vectors.
(define (vector-map proc v . vectors)
  (list->vector (apply map proc (map vector->list (cons v vectors)))))

;; Applies `proc` to the elements at each index of the vectors, in order.
(define (vector-for-each proc v . vectors)
  (apply for-each proc (map vector->list (cons v vectors))))

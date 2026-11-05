;;; The higher-order procedures of strings and vectors (R7RS 6.10), over their
;;; elements as lists, to the shortest's length.

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

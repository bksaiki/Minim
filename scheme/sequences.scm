;;; The higher-order procedures of strings (R7RS 6.10), over their chars as
;;; lists, to the shortest's length.

;; The string of `(proc c ...)` for the chars at each index of the strings.
(define (string-map proc s . strings)
  (list->string (apply map proc (map string->list (cons s strings)))))

;; Applies `proc` to the chars at each index of the strings, in order.
(define (string-for-each proc s . strings)
  (apply for-each proc (map string->list (cons s strings))))

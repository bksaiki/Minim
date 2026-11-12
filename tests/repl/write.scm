; write labels cycles; write-shared, every shared pair and vector;
; write-simple, nothing.
(define x (list 1 2))
(define c (list 1 2))
(set-cdr! (cdr c) c)
(begin (write (list x x)) (newline))
(begin (write-shared (list x x)) (newline))
(begin (write-shared (vector x (list x) x)) (newline))
(begin (write-shared c) (newline))
(begin (write-simple (list x x)) (newline))
(begin (write c) (newline))

; display labels cycles too.
(define s (list "a" #\b))
(set-cdr! (cdr s) s)
(begin (display s) (newline))

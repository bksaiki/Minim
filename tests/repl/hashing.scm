; equal? values hash equal.
(= (equal-hash '(1 2 (3 #(4 "x")))) (equal-hash (list 1 2 (list 3 (vector 4 (string-append "" "x"))))))
(= (equal-hash #(a "b" (c . d))) (equal-hash (vector 'a "b" (cons 'c 'd))))
(= (equal-hash 'abc) (equal-hash (string->symbol (string-append "ab" "c"))))
(= (equal-hash #\a) (equal-hash (integer->char 97)))
(= (equal-hash car) (equal-hash car))
(= (string-hash "caf\xe9;") (string-hash (list->string (list #\c #\a #\f #\xe9))))

; Different values usually hash differently.
(= (string-hash "a") (string-hash "b"))
(= (equal-hash '(1 2)) (equal-hash '(2 1)))
(= (equal-hash #(1 2)) (equal-hash '(1 2)))
(= (equal-hash 'abc) (equal-hash "abc"))

; Results are non-negative fixnums.
(fixnum? (equal-hash -5))
(>= (equal-hash -5) 0)
(>= (string-hash "") 0)
(fixnum? (equal-hash (lambda (x) x)))

; Hashing visits a bounded part of the structure, so cycles terminate.
(define c (list 1 2 3))
(set-cdr! (cdr (cdr c)) c)
(fixnum? (equal-hash c))
(define v (vector 1 2))
(vector-set! v 1 v)
(fixnum? (equal-hash v))
(fixnum? (equal-hash (make-vector 1000000 'x)))

(string-hash 'a)

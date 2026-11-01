; Error objects: the predicates and accessors.
(define e (%make-error-object "bad thing" '(1 (2) "three")))
(error-object? e)
(error-object-message e)
(error-object-irritants e)
(read-error? e)
(file-error? e)
e
(error-object-irritants (%make-error-object "none" '()))

; Not error objects.
(map error-object? (list 'x "bad" 5 '() (vector 1) car))
(map read-error? (list 'x "bad" 5))
(map file-error? (list 'x "bad" 5))

; Compared by identity.
(eq? e e)
(equal? e (%make-error-object "bad thing" '(1 (2) "three")))
(= (equal-hash e) (equal-hash e))

; They survive collections, contents and all.
(define es (map (lambda (i) (%make-error-object (number->string i) (list i))) '(1 2 3)))
(collect)
(map error-object-message es)
(map error-object-irritants es)
(collect)
(error-object-message e)
(error-object-irritants e)

; Bad arguments.
(error-object-message "bad")
(error-object-irritants 5)
(%make-error-object 'bad '())
(%make-error-object "bad" 5)

; eq? is identity.
(eq? 'a 'a)
(eq? (cons 1 2) (cons 1 2))

; eqv? is eq? until there are flonums and bignums.
(eqv? 'a 'a)
(eqv? 1 1)
(eqv? #\a #\a)
(eqv? '() '())
(eqv? (cons 1 2) (cons 1 2))
(eqv? "a" "a")

; equal? compares pairs, vectors, and strings by content.
(equal? 1 1)
(equal? 'a 'b)
(equal? '(1 2 3) (list 1 2 3))
(equal? '(1 2 3) '(1 2))
(equal? '(1 . 2) (cons 1 2))
(equal? '(1 . 2) '(1 . 3))
(equal? '((1 (2)) #(3)) (list (list 1 (list 2)) (vector 3)))
(equal? #(1 #(2 "x")) (vector 1 (vector 2 "x")))
(equal? #(1 2) #(1 2 3))
(equal? "caf\xe9;" "caf\xe9;")
(equal? "abc" "abd")
(equal? "" "")
(equal? 2 "2")
(equal? '(1) #(1))
(define p (cons 1 2))
(equal? p p)

; Bytevectors are equal? by their bytes.
(equal? #u8(1 2 3) (list->vector '(1 2 3)))
(equal? #u8(1 2 3) #u8(1 2 3))
(equal? #u8(1 2) #u8(1 2 3))
(equal? #u8() #u8())
(equal? '(#u8(255)) '(#u8(255)))
(= (equal-hash #u8(1 2)) (equal-hash #u8(1 2)))
(eq? #u8(1) #u8(1))

; Accessors and copies
(list-tail '(1 2 3) 0)
(list-tail '(1 2 3) 2)
(list-ref '(a b c) 1)
(list-copy '(1 2 3))
(list-copy '(1 2 . 3))
(list-copy '())
(define l (list 1 2))
(eq? (list-copy l) l)
(reverse '())
(reverse '(1 2 3))

; append copies all but the last argument, which is shared.
(append)
(append '(1))
(append '(1) '(2 3) '() '(4))
(append '(1) 2)
(define tail (list 3))
(eq? (cdr (append '(1) tail)) tail)

; Searching
(memq 'c '(a b c d))
(memq 'e '(a b c))
(memv 2 '(1 2 3))
(member "b" '("a" "b" "c"))
(member 2 '(1 2 3) (lambda (x y) (< x y)))
(assq 'b '((a 1) (b 2)))
(assv 2 '((1 one) (2 two)))
(assoc "b" '(("a" . 1) ("b" . 2)))
(assoc 2 '((1 . a) (5 . b)) (lambda (x y) (< x y)))
(assq 'z '((a 1)))

; Mapping stops at the shortest list.
(map (lambda (x) (* x x)) '(1 2 3))
(map + '(1 2 3) '(10 20 30))
(map + '(1 2 3) '(10 20))
(map list '(1 2) '(a b) '("x" "y"))
(map car '())
(for-each (lambda (x) (display x)) '(1 2 3))
(newline)
(for-each (lambda (a b) (display (list a b))) '(1 2) '(x y z))
(newline)

(filter (lambda (x) (< x 3)) '(1 5 2 4 0))
(filter pair? '())

; fold is from the left, fold-right from the right.
(fold cons '() '(1 2 3))
(fold-right cons '() '(1 2 3))
(fold + 0 '(1 2 3 4))
(fold-right list 'end '(1 2))

; Errors come from the primitives the library calls.
(list-ref '(1 2) 2)
(map 5 '(1))
(reverse 5)

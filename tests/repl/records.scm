; A record type and its records.
(define point (%make-record-type '<point> '(x y)))
point
(%record-ref point 0)
(%record-ref point 1)
(%record-ref point 2)
(define p (%make-record point))
p
(list (%record-ref p 0) (%record-ref p 1))
(%record-set! p 0 1)
(%record-set! p 1 '(2))
(list (%record-ref p 0) (%record-ref p 1))
(eq? (%record-rtd p) point)

; Predicates: a record of the type, not of another or not a record.
(define other (%make-record-type 'other '()))
(define o (%make-record other))
o
(list (%record? p point) (%record? o point) (%record? 5 point) (%record? point point))
(%record? point (%record-rtd point))
(%record-rtd point)
(eq? (%record-rtd (%record-rtd point)) (%record-rtd point))

; Records are equal? only when eq?.
(define q (%make-record point))
(%record-set! q 0 1)
(%record-set! q 1 '(2))
(list (equal? p p) (equal? p q) (= (equal-hash p) (equal-hash q)))

; They survive collections.
(define ps (map (lambda (i) (let ((r (%make-record point))) (%record-set! r 0 i) r)) '(1 2 3)))
(collect)
(map (lambda (r) (%record-ref r 0)) ps)

; Errors.
(%make-record-type "point" '(x))
(%make-record-type 'point '(x x))
(%make-record-type 'point '(x 1))
(%make-record-type 'point '(x . y))
(let ((l (list 'a 'b)))  ; cyclic, so printed by message only
  (set-cdr! (cdr l) l)
  (guard (e (#t (error-object-message e))) (%make-record-type 'point l)))
(%make-record 5)
(%make-record p)
(%make-record (%record-rtd point))
(%record-rtd 5)
(%record-ref 5 0)
(%record-ref p 2)
(%record-ref p -1)
(%record-set! 5 0 0)
(%record-set! p 2 0)
(%record-set! point 1 100)
(%record-ref point 1)

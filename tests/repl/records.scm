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

; define-record-type: R7RS 5.5's example.
(define-record-type <pare> (kons x y) pare? (x kar set-kar!) (y kdr))
(pare? (kons 1 2))
(pare? (cons 1 2))
(kar (kons 1 2))
(kdr (kons 1 2))
(let ((k (kons 1 2))) (set-kar! k 3) (kar k))
(kons 1 2)
<pare>

; A constructor naming some fields, in another order; the rest are #f.
(define-record-type node (make-node right key) node? (key node-key) (left node-left set-node-left!)
  (right node-right))
(define n (make-node 'r 'k))
(list (node-key n) (node-left n) (node-right n))
(set-node-left! n 'l)
(node-left n)

; No fields.
(define-record-type unit (make-unit) unit?)
(list (unit? (make-unit)) (unit? n) (eq? (make-unit) (make-unit)))

; Definitions in a body; two types with one name are distinct.
(define (make-type-pair)
  (define-record-type box (make-box v) box? (v box-v))
  (list make-box box?))
(define a (make-type-pair))
(define b (make-type-pair))
(list ((cadr a) ((car a) 1)) ((cadr b) ((car a) 1)))

; Hygiene: fields named like the globals the expansion uses.
(define-record-type thing (make-thing list make error) thing? (list thing-list) (make thing-make)
  (error thing-error))
(define t (make-thing 1 2 3))
(list (thing-list t) (thing-make t) (thing-error t))

; Errors.
(kar 5)
(set-kar! (cons 1 2) 3)
(kdr n)
(kons 1)
(define-record-type bad (make-bad z) bad? (x bad-x))
(define-record-type bad (make-bad) bad? (x bad-x) (x bad-x2))

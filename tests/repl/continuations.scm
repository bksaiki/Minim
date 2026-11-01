; An escape: the first negative element.
(call/cc (lambda (return) (for-each (lambda (x) (if (< x 0) (return x))) '(1 -2 3 -4)) 'none))
(call-with-current-continuation (lambda (return) 5))

; A continuation captured by one top-level form, invoked by a later one,
; finishes the earlier form's work, including its pending `+`.
(define k #f)
(+ 1 (call/cc (lambda (c) (set! k c) 1)))
(k 10)
(k 20)

; Re-entry within one form, several times.
(define n 0)
(define again #f)
(let ((x (call/cc (lambda (c) (set! again c) 0))))
  (set! n (+ n 1))
  (if (< x 3) (again (+ x 1)) (list x n)))

; Any number of values.
(call-with-values (lambda () (call/cc (lambda (c) (c 1 2)))) list)
(call-with-values (lambda () (call/cc (lambda (c) (c)))) list)

; Captured deep in a recursion, then re-entered there.
(define (deep n) (if (= n 0) (call/cc (lambda (c) (set! again c) 0)) (+ 1 (deep (- n 1)))))
(deep 10000)
(again 5)

(procedure? k)
k

; Errors.
(+ 1 (call/cc (lambda (c) (c 1 2))))
(call/cc 5)
(call/cc)
(call/cc (lambda () 1))
'done

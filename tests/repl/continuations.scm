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

; dynamic-wind runs `before`, the thunk, then `after`, and returns the
; thunk's values.
(define log '())
(define (note x) (set! log (cons x log)))
(define (ext name thunk)
  (dynamic-wind (lambda () (note (list 'in name))) thunk (lambda () (note (list 'out name)))))
(call-with-values (lambda () (ext 'v (lambda () (values 1 2)))) list)
(reverse log)

; An escape out of nested extents runs the `after`s innermost first.
(set! log '())
(call/cc (lambda (k) (ext 1 (lambda () (ext 2 (lambda () (k 'escaped)))))))
(reverse log)

; Re-entry runs the `before`s outermost first.
(define re #f)
(ext 1 (lambda () (ext 2 (lambda () (call/cc (lambda (c) (set! re c))) 'body))))
(set! log '())
(re 'again)
(reverse log)

; A jump between sibling extents leaves one and enters the other.
(define kb #f)
(ext 'b (lambda () (call/cc (lambda (c) (set! kb c))) 'b-done))
(set! log '())
(ext 'a (lambda () (kb 'jump)))
(reverse log)

; An unhandled error runs `after`, and a later jump runs no stale one.
(set! log '())
(ext 'e (lambda () (car 5)))
(reverse log)
(set! log '())
(k 30)
(reverse log)

; Errors.
(+ 1 (call/cc (lambda (c) (c 1 2))))
(call/cc 5)
(call/cc)
(call/cc (lambda () 1))
'done

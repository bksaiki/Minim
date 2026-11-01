; values returns any number of values; the REPL prints each on its own line.
(values 1 2 3)
(values 'a)
(values)
(+ 1 (values 2))

; call-with-values passes the producer's values to the consumer.
(call-with-values (lambda () (values 1 2)) +)
(call-with-values (lambda () (values 1 2)) (lambda args args))
(call-with-values (lambda () (values)) list)
(call-with-values (lambda () 5) (lambda (x) (* x x)))
(call-with-values values list)
(call-with-values (lambda () (apply values '(1 2 3))) list)
(call-with-values (lambda () (call-with-values (lambda () (values 1 2)) values)) list)

; values is a procedure like any other.
(apply values '(1 2))
(map values '(1 2 3))

; A non-final form's values are discarded.
(begin (values 1 2) 3)
((lambda () (values) (values 1 2) 4))
(load "load/values.scm")
after-values

; Errors: any count but one where one value is expected.
(+ 1 (values 2 3))
(list (values))
(if (values 1 2) 'yes 'no)
(define x (values 1 2))
(define y 0)
(set! y (values))
y
(call-with-values (lambda () (values 1 2)) (lambda (a) a))
(call-with-values 1 list)
(call-with-values list)
'done

; define-values at top level, and replacing a global macro.
(define-values (p q) (values 1 2))
(list p q)
(define-values (h . t) (values 1 2 3))
(list h t)
(define-values all (values 1 2))
all
(define-values () (values))
(define-syntax m (syntax-rules () ((_) 'macro)))
(define-values (m) (values 'variable))
m

; Internal define-values, mixed with define; every definition sees the others.
((lambda ()
   (define a 1)
   (define-values (b c) (values (+ a 1) (lambda () d)))
   (define d 3)
   (list a b (c))))
(let () (define-values (x . y) (values 1 2)) (list x y))

; let-values inits see the outer scope; letrec-values inits see the new one.
(define v 'outer)
(let-values (((v) (values 'inner)) ((w) (values v))) (list v w))
(letrec-values (((ev? od?)
                 (values (lambda (n) (if (= n 0) #t (od? (- n 1))))
                         (lambda (n) (if (= n 0) #f (ev? (- n 1)))))))
  (ev? 10))
(let*-values (((a b) (values 1 2)) ((c) (values (+ a b)))) (list a b c))
(let*-values () 'empty)
(let-values (((when) (values 5))) when)  ; a formal shadows a macro

; Hygiene: a macro's let-values binding doesn't capture the user's t, and a
; user's binding doesn't capture the macro's free `list`.
(define-syntax add-ten (syntax-rules () ((_ e) (let-values (((t) (values 10))) (+ t e)))))
(define t 1)
(add-ten t)
(define-syntax wrap (syntax-rules () ((_ e) (list e))))
(let-values (((list) (values vector))) (wrap 1))

; Errors.
(if #t (define-values (z) 1))
(define-values (z 1) 2)
(define-values (z))
(let-values (((z) 1)))
(let-values ((z)) z)
(let-values (((z 1) 2)) z)
(letrec-values (((z) 1) . r) z)
(define-values (z1 z2) (values 1))
'done

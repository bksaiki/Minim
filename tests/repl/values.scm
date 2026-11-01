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

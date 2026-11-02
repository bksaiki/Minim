; Self-evaluating data.
42
"s"
#\a
#f
#(1 x)

; Core forms.
'(1 x)
(quote x)
(if #t 1 2)
(if #f 1 2)
(if 0 1 2)
(if #f 1)
(define x 5)
x
(set! x 2)
x
(begin 1 2 3)
(begin)
((lambda (x y) (+ x y)) 1 2)
((lambda (x . r) r) 1 2 3)

; Closures capture their environment.
(define (adder n) (lambda (x) (+ x n)))
((adder 3) 4)
(define (shadow x) (set! x 5) x)
(shadow 0)
x

; Errors.
y
(set! y 1)
(5)
((lambda (x) x))
((lambda (x . r) x))
(car)
()
(if)
(quote 1 2)
(define)
(define x)
(define x 1 2)
(define (1) 1)
(set! 1 2)
(lambda (1) 1)
(lambda (x))
(+ 1 . 2)

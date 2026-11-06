; Data print back in write syntax.
42
-7
'foo
"hi\n"
#\a #\space
'(1 2 . 3)
''x
'`(a ,b ,@c)
#(1 #(2) "s")
#t #false

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

; A local variable can shadow keywords and special forms.
((lambda (if) (if 1)) list)
((lambda (quote) quote) 5)
((lambda (unquote) `(,unquote)) 7)
((lambda (lambda) (lambda 2)) list)

; A closure keeps its own state.
(define make-counter
  (lambda ()
    (define n 0)
    (lambda () (set! n (+ n 1)) n)))
(define c (make-counter))
(c)
(c)

; Booleans and procedures.
(not #f)
(not 0)
(procedure? car)
(procedure? (lambda () 1))
(procedure? 'car)
(boolean? #f)
(boolean? '())

; Application.
(apply + '(1 2 3))
(apply + 1 2 '(3 4))
(apply list '())
(apply apply (list + '(1 2)))
(apply (lambda (a . r) r) 1 '(2 3))
(apply + 1 2)
(apply car '(1 2))

; Output.
(write "hi\n")
(newline)
(write 'x)
(newline)
(display "hi")
(newline)
(display '(1 "a" #\b))
(newline)

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

; The REPL reports errors on stderr and continues: a read error, then an
; evaluation error.
)
'(after error)
(car 5)
(cdr '(1 2))

; The REPL's command line.
(command-line)

; void, after Chez.
(void)
(list (void))
(void 1)

; identity, Minim's own.
(identity 'x)
(map identity '(1 2))

; procedure-rename, after Racket's: a copy that prints with another name.
(define (sq x) (* x x))
(define sq2 (procedure-rename sq 'square))
sq2
(sq2 3)
sq
(procedure-rename car 'first)
((procedure-rename car 'first) '(1 2))
((procedure-rename apply 'app) + '(1 2))
((procedure-rename (case-lambda ((x) 1) ((x y) 2)) 'cl) 1 2)
(parameter? (procedure-rename (make-parameter 1) 'p))
exact->inexact
((procedure-rename car 'first))
(procedure-rename 1 'x)
(procedure-rename car "x")

; Binding names an unnamed closure; a named one keeps its name, and an
; argument doesn't name one.
(define (f x) x)
f
(define (make-adder n) (lambda (x) (+ x n)))
(define add1 (make-adder 1))
(define add2 (make-adder 2))
(list add1 add2)
(define g f)
g
(let ((h (lambda () 1))) h)
(letrec ((h (lambda () 1))) h)
(let loop ((i 0)) loop)
(define-values (p q) (values (lambda () 1) (lambda () 2)))
(list p q)
(define (outer) (define (inner) 1) inner)
(outer)
(lambda (x) x)
((lambda (p) p) (lambda () 1))
(map (lambda (p) p) (list (lambda () 1)))
(f 1 2)
(read-char (current-input-port) 1)

; Redefining a built-in shadows it for the user's code only: the prelude and
; the expander keep the original.
(define (car x) 'mine)
(car '(1 2))
(map (lambda (x) (* x 10)) '(1 2 3))
(let ((a 1) (b 2)) (cond ((= a 2) 'no) (else (list a b))))
(define (map f lst) 'my-map)
(map car '((1)))
(let loop ((i 0)) (if (< i 3) (loop (+ i 1)) i))

; A user's own definition is redefined in place: closures over it see the new
; value.
(define (greet) 'hello)
(define (call-greet) (greet))
(define (greet) 'hi)
(call-greet)
(define x 1)
(define (get-x) x)
(set! x 2)
(get-x)

; A macro the user defines shadows a built-in too, and a variable can then
; replace it.
(define-syntax when (syntax-rules () ((_ c e) (if c 'my-when #f))))
(when #t 1)
(unless #f 'still-the-system-unless)
(define when 5)
when

; A definition a macro inserts gets a fresh global, unless it names the
; user's own variable.
(define-syntax def-hidden (syntax-rules () ((_ v) (begin (define hidden v) hidden))))
(def-hidden 7)
hidden
(define-syntax def-x (syntax-rules () ((_ v) (define x v))))
(def-x 3)
x

; Imported bindings are immutable; the user's own are not.
(set! cdr 5)
(cdr '(1 2))
(set! map 5)
map

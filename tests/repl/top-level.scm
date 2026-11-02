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

; Imported bindings are immutable; the user's own are not.
(set! cdr 5)
(cdr '(1 2))
(set! map 5)
map

; Internals aren't visible: the REPL sees only what it imports, (minim).
%expand
(%set-handlers! 5)
(%record-ref 'x 0)
(list (let ((a 1)) a)
      (guard (e (#t 'caught)) (raise 'x))
      (case 2 ((1) 'one) ((2) 'two))
      (let () (define-record-type point (make-point x) point? (x point-x)) (point-x (make-point 7))))

; A user's internal-looking name is the user's own.
(define (%expand x) (list 'mine x))
(%expand 1)
(let ((a 1)) a)

; A name used before its definition means the definition.
(define (later) (defined-after))
(define (defined-after) 'after)
(later)

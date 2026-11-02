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

; A library: procedures, a renamed export, and a macro, imported and used.  A
; definition may refer to one further down.
(define-library (stack)
  (export make-stack push! (rename stack-top top) stack-empty? with-stack)
  (import (scheme base))
  (begin
    (define (make-stack) (list 'stack))
    (define (push! s x) (set-cdr! s (cons x (cdr s))))
    (define (stack-top s) (if (stack-empty? s) (error "empty stack") (cadr s)))
    (define (stack-empty? s) (null? (cdr s)))
    (define-syntax with-stack
      (syntax-rules () ((_ s body ...) (let ((s (make-stack))) body ... s))))))
(import (stack))
(define s (make-stack))
(push! s 1)
(list (top s) (stack-empty? s))
(with-stack t (push! t 5) (push! t 6))
(top (make-stack))

; Hygiene across libraries: a macro's reference to the library's private
; definition, and its own uses of `if` and `list`, mean what they do in the
; library, whatever the importer binds.
(define-library (counter)
  (export count!)
  (import (scheme base))
  (begin
    (define n 0)
    (define (bump) (set! n (+ n 1)) n)
    (define-syntax count! (syntax-rules () ((_) (if #t (list (bump)) #f))))))
(import (counter))
(define (bump) 'importer-bump)
(let ((if 'shadowed) (list 'shadowed)) (count!))
(count!)
n
bump

; A library importing another; its body runs in order when it is defined.
(define-library (greeter)
  (export greet)
  (import (scheme base) (only (stack) make-stack push! top))
  (begin
    (define messages (make-stack))
    (push! messages "hello")
    (define (greet) (top messages))))
(import (greeter))
(greet)

; Import sets.
(import (prefix (only (stack) make-stack) my-))
(my-make-stack)
(import (rename (only (stack) push!) (push! stack-push!)))
(let ((s (make-stack))) (stack-push! s 3) (top s))
(import (except (counter)))
(import (only (scheme base) car) (prefix (only (scheme base) cdr) base-))
(base-cdr '(1 2))

; Errors.
(import (only (stack) nothing))
(import (rename (stack) (nothing x)))
(import (nowhere))
(define-library (bad) (export missing) (import (scheme base)) (begin (define x 1)))
(define-library (late) (export f) (import (scheme base)) (begin (define (f) (undefined-thing))))
(import (late))
(f)
(define-library (no-base) (export g) (begin (define (g) 1)))

; Standard libraries: a library sees only what it imports.
(define-library (shout)
  (export shout first)
  (import (only (scheme base) define quote) (scheme write))
  (begin
    (define (shout x) (display x) (display "!"))
    (define (first x) (car x))))
(import (shout))
(shout 'hey)
(first '(1 2))
(import (scheme base) (scheme write) (scheme file) (scheme load) (scheme process-context))
(map car '((1) (2)))
(list (file-exists? "libraries.scm") (procedure? load) (get-environment-variable "MINIM_UNSET"))
(import (scheme char))

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
(import (only))
(import (rename (stack) (push!)))
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
(char-upcase #\a)
(import (scheme lazy))
(force (delay 'lazy))
(import (scheme inexact))
(list (finite? 1.5) (infinite? -inf.0) (nan? +nan.0))

; Library files: an unknown library loads from its .sld file, once, and
; includes files relative to it.
(import (lib shapes) (lib point))
(import (lib point))
(point->list (make-point 1 2))
(distance-squared (make-point 0 0) (make-point 3 4))
(import (lib missing))
(import (lib wrong))
(import (lib bad-include))

; A library file's include-library-declarations and include-ci, relative to
; its directory.
(import (lib folded))
(shout 2)

; (scheme r5rs): R5RS's names from several R7RS libraries, and its syntax;
; R7RS additions like `when` and `error` are unbound in it.
(define-library (old-style)
  (export run fail)
  (import (scheme r5rs))
  (begin
    (define-syntax swap!
      (syntax-rules ()
        ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp)))))
    (define (run)
      (let ((x 1) (y 2))
        (swap! x y)
        (write (cond ((> x y) => (lambda (t) `(swapped ,x ,y))) (else 'no)))
        (write (list (exact->inexact x) (inexact->exact 1.0)))
        (newline)))
    (define (fail) (when #t (error "unreachable")))))
(import (old-style))
(run)
(fail)

; (minim): every public name, the standard libraries' and Minim's own.
(define-library (uses-minim)
  (export g)
  (import (minim))
  (begin
    (define (g) (list (symbol? (gensym)) (fold + 0 '(1 2 3)) (when #t 'when)))))
(import (uses-minim))
(g)

; (minim system): every system binding, internals included, only whole.
(define-library (uses-system)
  (export h)
  (import (minim system))
  (begin
    (define (h) (list (%bound? 'car) (car '(1))))))
(import (uses-system))
(h)
(import (only (minim system) car))
(import (prefix (minim system) s:))

; (scheme case-lambda).
(define-library (arity)
  (export arity)
  (import (scheme base) (scheme case-lambda))
  (begin
    (define arity
      (case-lambda
        (() 0)
        ((x) 1)
        ((x . r) 'many)))))
(import (arity))
(list (arity) (arity 1) (arity 1 2))

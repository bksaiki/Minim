; How the boot file's generator (src/boot.scm) writes a top-level form.
(load "../../src/boot.scm")

; Fresh names become name.N, distinct per name and numbered within each form;
; globals and keywords stay.
(%boot-form '(define f (lambda (x) (lambda (x) x))))
(%boot-form '(define g (lambda (x y) (let ((x y)) x))))
(%boot-form '(define h (lambda (x) x)))

; A begin is spliced, a macro use expanded.
(%boot-form '(begin (define a 1) (begin (define b 2) (when a b))))

; A define-syntax becomes a call that rebuilds the macro, which is also defined
; for the forms after it.
(%boot-form '(define-syntax twice (syntax-rules () ((_ e) (begin e e)))))
(%boot-form '(define n (lambda () (twice 1))))

; A macro that defines a fresh global can't be written.
(define-syntax hide (syntax-rules () ((_ v) (define hidden v))))
(%boot-form '(hide 1))

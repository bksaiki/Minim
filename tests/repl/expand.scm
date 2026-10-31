; Without %expand, forms run as is.
(define n 0)
(define %expand
  (lambda (form)
    (if (if (pair? form) (eq? (car form) 'twice) #f)
        (list 'begin (car (cdr form)) (car (cdr form)))
        form)))

; Each top-level form goes through %expand.
(twice (set! n (+ n 1)))
n

; So does each form of a loaded file, and a %expand redefined by one form
; applies to the next.
(load "load/expand.scm")
n
(thrice (set! n (+ n 1)))
n

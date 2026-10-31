(twice (set! n (+ n 1)))
(define %expand
  (lambda (form)
    (if (if (pair? form) (eq? (car form) 'thrice) #f)
        (list 'begin (car (cdr form)) (car (cdr form)) (car (cdr form)))
        form)))
(thrice (set! n (+ n 1)))

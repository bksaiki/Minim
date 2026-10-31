; A local macro's free identifier means the binding visible at its definition.
((lambda (x)
   (let-syntax ((get-x (syntax-rules () ((_) x))))
     ((lambda (x) (get-x)) 'inner)))
 'outer)

; letrec-syntax macros see themselves and each other; let-syntax ones see the
; outer environment, where these names are unbound.
(letrec-syntax ((my-and (syntax-rules ()
                          ((_) #t)
                          ((_ e) e)
                          ((_ e r ...) (if e (my-and r ...) #f)))))
  (my-and 1 2 3))
(letrec-syntax ((a (syntax-rules () ((_) (b))))
                (b (syntax-rules () ((_) 'b))))
  (a))
(let-syntax ((my-and (syntax-rules ()
                       ((_) #t)
                       ((_ e) e)
                       ((_ e r ...) (if e (my-and r ...) #f)))))
  (my-and 1 2 3))
(let-syntax ((a (syntax-rules () ((_) (b))))
             (b (syntax-rules () ((_) 'b))))
  (a))

; An internal define-syntax serves definitions before and after it.
(define f
  (lambda ()
    (define before (lambda () (twice 1)))
    (define-syntax twice (syntax-rules () ((_ e) (list e e))))
    (define after (twice 2))
    (list (before) after)))
(f)

; A local macro shadows a global one; a local variable shadows a macro.
(define-syntax which (syntax-rules () ((_) 'global)))
(let-syntax ((which (syntax-rules () ((_) 'local))))
  (which))
(which)
((lambda (which) (which)) (lambda () 'variable))

; A let-syntax body is a body: it may define, and its value is the last
; expression's.
(let-syntax ((one (syntax-rules () ((_) 1))))
  (define two (+ (one) (one)))
  (list (one) two))

; Errors.
(let-syntax ((m (syntax-rules () ((_) 1)))) m)
(let-syntax ((m)) 1)
(let-syntax ((m (syntax-rules () ((_) 1)))))
(lambda () (define-syntax m) 1)

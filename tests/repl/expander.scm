; Quasiquote: unquote and unquote-splicing at the head, middle, and tail.
(define x 1)
(define l '(2 3))
`(,x b c)
`(a ,x c)
`(a b ,x)
`(,@l b)
`(a ,@l c)
`(a ,@l)
`(,@l)
`(a ,@'() b)
`(a . ,x)
`(a ,x . ,l)
`#(a ,x ,@l)
`#(a b)
`x
`,x
`(1 ,(+ 1 1) ,@(list 3 4))

; Nested quasiquote: inner unquotes stay, unless at depth 0.
`(a `(b ,(c ,x)))
`(a `(b ,,x))
``,,x
`(a `(b ,@,@l))

; Unquoted parts run left to right.
(define log '())
(define note (lambda (v) (set! log (cons v log)) v))
`(,(note 1) ,@(list (note 2)) ,(note 3))
log

; A local variable can shadow keywords and special forms.
((lambda (if) (if 1)) list)
((lambda (quote) quote) 5)
((lambda (unquote) `(,unquote)) 7)
((lambda (lambda) (lambda 2)) list)

; Internal definitions shadow globals throughout their body and can refer to
; each other, whatever their order.
(define y 'global)
(define f
  (lambda ()
    (define g (lambda () y))
    (define y 'local)
    (g)))
(f)
y
(define parity
  (lambda (n)
    (define even? (lambda (n) (if (= n 0) #t (odd? (- n 1)))))
    (define odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))
    (even? n)))
(parity 10)
(define h
  (lambda ()
    (begin (define a 1) (begin (define b 2)))
    (+ a b)))
(h)
((lambda (x) (define x 2) x) 1)
((lambda (a) (define b (+ a 1)) b) 1)

; Quoted data is left alone.
'(lambda (if) (if 1))
'(define)
(quote `(a ,b))

; Errors.
`(a ,@5)
`(a . ,@l)
`,@l
(define bad (lambda () (if)))
(lambda () (display 1) (define z 1) z)
(lambda () (begin))
(lambda () (define z 1))
(let () (define z 1))
(let-values () (define-values (z) 1))
(lambda () (define-syntax m (syntax-rules () ((_) 1))))
(if (define z 1) 2)
(set! if 1)
(lambda (x 1) x)

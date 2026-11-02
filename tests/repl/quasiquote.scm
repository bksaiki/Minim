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

; Quoted data is left alone.
'(lambda (if) (if 1))
'(define)
(quote `(a ,b))

; Errors.
`(a ,@5)
`(a . ,@l)
`,@l

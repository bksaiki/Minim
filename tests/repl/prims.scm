; Pairs
(define p (cons 1 2))
(set-car! p 10)
(set-cdr! p '(20))
p
(pair? p)
(pair? '())
(null? '())
(null? p)
(list)
(list 1 'a "b")
(set-car! 5 1)

; Identity and types
(eq? 'a 'a)
(eq? (cons 1 2) (cons 1 2))
(not #f)
(not 0)
(symbol? 'a)
(symbol? "a")
(procedure? car)
(procedure? (lambda () 1))
(procedure? 'car)
(boolean? #f)
(boolean? '())

; Vectors
(define v (make-vector 3))
v
(make-vector 2 'x)
(vector-set! v 0 'a)
(vector-ref v 0)
(vector-length v)
(vector 1 (vector 2) "s")
(vector)
(vector? v)
(vector? '(1))
(vector-ref '(1) 0)
(vector-ref v 3)
(vector-set! v -1 0)
(make-vector -1)

; Strings and symbols
(string-length "caf\xe9;")
(string-ref "caf\xe9;" 3)
(string? "s")
(string? 's)
(symbol->string 'abc)
(string->symbol "abc")
(eq? (string->symbol "abc") 'abc)
(string-ref "abc" 3)
(string-length 'abc)
(symbol->string "abc")

; Output
(write "hi\n")
(newline)
(write 'x)
(newline)

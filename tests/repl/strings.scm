; Building
(make-string 3 #\x)
(make-string 2)
(make-string 0)
(define s (make-string 3 #\a))
(string-set! s 1 #\xe9)
s
(string-append)
(string-append "ab")
(string-append "ab" "" "c\xe9;" "d")
(substring "hello" 1 3)
(substring "hello" 0 5)
(substring "hello" 2 2)
(substring "caf\xe9;" 3 4)

; Comparisons are lexicographic by code point.
(string=? "abc" "abc" "abc")
(string=? "abc" "abd")
(string=? "")
(string<? "abc" "abd")
(string<? "ab" "abc")
(string<? "abc" "ab")
(string<? "Z" "a")
(string>? "b" "a")
(string<=? "a" "a" "b")
(string>=? "a" "b")
(string<? "caf\xe9;" "cafe")

; Conversions
(string->list "")
(string->list "a\xe9;c")
(list->string '())
(list->string (list #\a #\xe9 #\c))
(equal? (list->string (string->list "round trip")) "round trip")

; Errors
(string-set! (symbol->string 'abc) 0 #\x)
(string-set! s 3 #\a)
(string-set! s 0 "a")
(string-append "a" 'b)
(substring "hello" 2 6)
(substring "hello" 3 2)
(list->string (list #\a 1))
(list->string (cons #\a #\b))
(make-string -1)
(string<? "a" 'b)

; Length and access.
(string-length "caf\xe9;")
(string-ref "caf\xe9;" 3)
(string? "s")
(string? 's)
(string-ref "abc" 3)
(string-length 'abc)

; Construction, copying, and filling, each over an optional range.
(string)
(string #\a #\xe9 #\c)
(string-copy "hello")
(string-copy "hello" 2)
(string-copy "hello" 1 3)
(let ((s "abc")) (eq? s (string-copy s)))
(string-copy (symbol->string 'abc))
(string->list "hello" 2)
(string->list "hello" 1 3)
(define t (make-string 5 #\x))
(string-fill! t #\-)
t
(string-fill! t #\a 3)
t
(string-fill! t #\b 1 2)
t
(define u (string-copy "abcde"))
(string-copy! u 1 "12345" 0 2)
u
(string-copy! u 0 "-----" 3)
u
(string-copy! u 1 u 0 3)
u
(string-copy! u 0 u 2)
u

(string #\a "b")
(string-copy "abc" 2 1)
(string-copy "abc" 4)
(string->list "abc" 1 4)
(string-fill! (symbol->string 'abc) #\x)
(string-fill! t 1)
(string-copy! u 4 "abc")
(string-copy! (symbol->string 'abc) 0 "x")

; Uninterned symbols print like their names but are distinct.
(define g (gensym "x"))
g
(symbol? g)
(eq? g 'x)
(eq? g (gensym "x"))
(eq? (string->symbol (symbol->string g)) 'x)
(eq? (gensym) (gensym))
(gensym 'x)

; Symbols and strings.
(symbol? 'a)
(symbol? "a")
(symbol->string 'abc)
(string->symbol "abc")
(eq? (string->symbol "abc") 'abc)
(symbol->string "abc")

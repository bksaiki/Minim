; The internals this file tests are in (minim system).
(import (minim system))

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
(string->symbol "a b")
(begin (write (string->symbol "1")) (newline))
(begin (display (string->symbol "a b")) (newline))
(symbol->string "abc")

; Property lists, keys compared by eq?.
(define env (vector 'env))
(%getprop 'p env #f)
(%putprop! 'p env 1)
(%putprop! 'p 'k 2)
(%getprop 'p env #f)
(%getprop 'p 'k #f)
(%getprop 'p (vector 'env) #f)
(%getprop 'q env #f)
(%putprop! 'p env 3)
(%getprop 'p env #f)
(%getprop 'q env 'none)
(%getprop (gensym "p") 'k #f)
(%getprop "p" 'k #f)
(%putprop! 1 'k 2)

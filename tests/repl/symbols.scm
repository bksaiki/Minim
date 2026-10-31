; Uninterned symbols print like their names but are distinct.
(define g (gensym "x"))
g
(symbol? g)
(eq? g 'x)
(eq? g (gensym "x"))
(eq? (string->symbol (symbol->string g)) 'x)
(eq? (gensym) (gensym))
(gensym 'x)

; An alias remembers its identifier and environment.
(define a (%make-alias 'if '((y . z))))
a
(eq? a 'if)
(%alias-original a)
(%alias-env a)
(define b (%make-alias a '()))
b
(eq? (%alias-original b) a)
(%alias-original (%alias-original b))
(%alias-env b)
(%alias-original 'if)
(%alias-env g)
(%alias-original 5)
(%make-alias "if" '())

; %bound? is whether a global holds a value.
(%bound? 'car)
(%bound? 'not-yet)
(define not-yet 1)
(%bound? 'not-yet)
(%bound? g)
(%bound? a)
(%bound? 5)

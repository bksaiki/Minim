; Cyclic literals in code evaluate to themselves.
'#0=(a . #0#)
'#0=#(1 #0#)
(define c '#0=(1 2 . #0#))
(eq? c (cddr c))

; A macro's quoted alias is stripped beside a cyclic argument, which is copied
; with its cycles and sharing.
(define-syntax q
  (syntax-rules ()
    ((_ x) '(a x))))
(q #0=(1 . #0#))
(eq? (car (q 1)) 'a)
(begin (write-shared (q (#0=(1) #0#))) (newline))

; A datum with no alias is left as read.
(define d (read (open-input-string "(x #(y))")))
(eq? d (eval (list 'quote d) (interaction-environment)))

; A cyclic literal in a library body.
(define-library (cyclic)
  (export cycle)
  (import (scheme base))
  (begin (define cycle '#0=(x y . #0#))))
(import (cyclic))
cycle

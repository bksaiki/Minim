; (features): a fresh list of symbols, naming R7RS, Minim, and the byte order.
(define fs (features))
(list? fs)
(null? (filter (lambda (f) (not (symbol? f))) fs))
(list (and (memq 'r7rs fs) #t) (and (memq 'minim fs) #t))
(not (eq? (and (memq 'little-endian fs) #t) (and (memq 'big-endian fs) #t)))
(eq? fs (features))

; cond-expand: the forms of the first clause whose requirement holds.
(cond-expand (minim 'identifier) (else 'no))
(cond-expand ((and r7rs minim) 'and) (else 'no))
(cond-expand ((and) 'empty-and) (else 'no))
(cond-expand ((or no-such minim) 'or) (else 'no))
(cond-expand ((or) 'no) (else 'empty-or))
(cond-expand ((not no-such) 'not) (else 'no))
(cond-expand (no-such 'no) (else 'else))
(cond-expand (minim 'first) (r7rs 'second))

; (library name): whether it is defined or its file is found, loading nothing.
(cond-expand ((library (scheme base)) 'defined) (else 'no))
(cond-expand ((library (lib point)) 'found) (else 'no))
(cond-expand ((library (no such)) 'no) (else 'missing))

; Definitions splice at top level and at the start of a body.
(cond-expand (minim (define top-a 1) (define top-b (+ top-a 1))))
(list top-a top-b)
((lambda () (cond-expand (minim (define x 3))) (* x 2)))

; The expansion's begin is the system's, whatever the user binds.
(let ((begin list)) (cond-expand (minim 'hygienic)))

; In define-library, the chosen clause's declarations splice in.
(define-library (picked)
  (export which)
  (cond-expand
    (no-such (import (no such library)))
    (minim (import (scheme base))))
  (begin (define which 'minim)))
(import (picked))
which

; Errors: a malformed requirement, else not last, no clause matching.
(cond-expand ((nand a b) 1))
(cond-expand ((not a b) 1))
(cond-expand (else 1) (minim 2))
(cond-expand (no-such 1))

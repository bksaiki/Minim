; (features): a fresh list of symbols, naming R7RS, Minim, and the byte order.
(define fs (features))
(list? fs)
(null? (filter (lambda (f) (not (symbol? f))) fs))
(list (and (memq 'r7rs fs) #t) (and (memq 'minim fs) #t))
(not (eq? (and (memq 'little-endian fs) #t) (and (memq 'big-endian fs) #t)))
(eq? fs (features))

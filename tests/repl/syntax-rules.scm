; Literals match identifiers with the same binding.
(define-syntax arrow
  (syntax-rules (=>)
    ((_ a => b) (list a b))
    ((_ a x b) 'no-arrow)))
(arrow 1 => 2)
(arrow 1 + 2)
((lambda (=>) (arrow 1 => 2)) 0)

; `_`, dotted patterns, and rules tried in order.
(define-syntax second (syntax-rules () ((_ _ b . _) b)))
(second 1 2 3)
(define-syntax rest-of (syntax-rules () ((_ a . r) 'r)))
(rest-of 1 2 3)
(define-syntax count
  (syntax-rules ()
    ((_) 'none)
    ((_ x) 'one)
    ((_ x . y) 'many)))
(list (count) (count a) (count a b))

; Ellipses: single, empty, nested, and followed by more patterns.
(define-syntax my-list (syntax-rules () ((_ x ...) (list x ...))))
(my-list 1 2 3)
(my-list)
(define-syntax flatten (syntax-rules () ((_ (x ...) ...) '(x ... ...))))
(flatten (1 2) () (3))
(define-syntax groups (syntax-rules () ((_ (k v ...) ...) '((k (v ...)) ...))))
(groups (a 1 2) (b) (c 3))
(define-syntax last (syntax-rules () ((_ x ... y) 'y)))
(last 1 2 3)
(define-syntax ends (syntax-rules () ((_ a b ... c d) '(a c d (b ...)))))
(ends 1 2 3 4 5)
(ends 1 2 3)

; Vector patterns and templates.
(define-syntax vec (syntax-rules () ((_ #(a b ...)) '#(b ... a))))
(vec #(1 2 3))

; A custom ellipsis, and (... ...) for a literal ellipsis.
(define-syntax my-list2 (syntax-rules ::: () ((_ x :::) (list x :::))))
(my-list2 4 5)
(define-syntax dots (syntax-rules () ((_ x ...) '((x (... ...)) ...))))
(dots 1 2)

; A macro whose expansion uses another macro.
(define-syntax my-or
  (syntax-rules ()
    ((_ a b) ((lambda (t) (if t t b)) a))))
(define-syntax my-or3 (syntax-rules () ((_ a b c) (my-or a (my-or b c)))))
(my-or3 #f #f 3)

; Hygiene: the template's `t` is a different variable, its `if` is the special
; form, its binding is invisible to the user's expression, and its quoted
; symbols print as written.
((lambda (t) (my-or #f t)) 5)
((lambda (if) (my-or #f 1)) list)
(define t 'user-t)
(define-syntax with-t (syntax-rules () ((_ e) ((lambda (t) e) 'macro-t))))
(with-t t)
(define-syntax quoted (syntax-rules () ((_) '(if t x))))
(quoted)
(define-syntax swap!
  (syntax-rules ()
    ((_ a b) ((lambda (tmp) (set! a b) (set! b tmp)) a))))
(define tmp 1)
(define other 2)
(swap! tmp other)
(list tmp other)

; Top-level definitions inserted by a macro: an existing global is assigned,
; an unbound one becomes a private global.
(define-syntax define-five (syntax-rules () ((_) (define five 5))))
(define five 0)
(define-five)
five
(define-syntax define-secret
  (syntax-rules ()
    ((_ get) (begin (define secret 42) (define get (lambda () secret))))))
(define-secret get-secret)
(get-secret)
secret

; A definition replaces a macro of the same name.
(define my-list 'variable)
my-list

; Errors.
(my-or 1)
(define-syntax bad (lambda (x) x))
(define-syntax must-be-pair
  (syntax-rules ()
    ((_ (a . b)) 'pair)
    ((_ x) (syntax-error "not a pair" x))))
(must-be-pair (1 . 2))
(must-be-pair 5)
(define-syntax two-args (syntax-rules () ((_) (syntax-error "two" 1 2))))
(two-args)
(define-syntax depth (syntax-rules () ((_ x ...) 'x)))
(depth 1 2)

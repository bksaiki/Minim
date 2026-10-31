;;; Library procedures written in Scheme, evaluated by minim_init.

;;; Expander.  (%expand form) rewrites a top-level form into core forms; the
;;; evaluator calls it on every top-level form once it is defined, last in this
;;; section.  The expander uses only core forms (quote, if, define, set!,
;;; lambda, begin) and primitives.  See docs/design/macros.md.

;; Global macros: an association list of (symbol . transformer).
(define %macros '())

(define %map
  (lambda (f lst)
    (if (null? lst)
        '()
        (cons (f (car lst)) (%map f (cdr lst))))))

(define %reverse
  (lambda (lst)
    (define loop
      (lambda (lst acc)
        (if (null? lst) acc (loop (cdr lst) (cons (car lst) acc)))))
    (loop lst '())))

(define %append
  (lambda (a b)
    (if (null? a) b (cons (car a) (%append (cdr a) b)))))

;; Length of `x` plus `n`, or #f if `x` isn't a proper list.
(define %length
  (lambda (x n)
    (if (null? x) n (if (pair? x) (%length (cdr x) (+ n 1)) #f))))

(define %vector->list
  (lambda (v)
    (define loop
      (lambda (i acc)
        (if (= i 0) acc (loop (- i 1) (cons (vector-ref v (- i 1)) acc)))))
    (loop (vector-length v) '())))

;; A syntactic environment is a list of frames, innermost first.  A frame is a
;; one-element list holding an association list of (identifier . meaning); a
;; variable's meaning is its fresh name.

(define %extend
  (lambda (env)
    (cons (list '()) env)))

;; Binds the identifier `id` in the innermost frame of `env` to a fresh name,
;; which it returns.
(define %bind-variable!
  (lambda (env id)
    (define name (gensym (symbol->string id)))
    (set-car! (car env) (cons (cons id name) (car (car env))))
    name))

;; Meaning of `id` in `env`, or #f.
(define %lookup
  (lambda (id env)
    (if (null? env) #f (%lookup-in id (car (car env)) (cdr env)))))

(define %lookup-in
  (lambda (id bindings env)
    (if (null? bindings)
        (%lookup id env)
        (if (eq? (car (car bindings)) id)
            (cdr (car bindings))
            (%lookup-in id (cdr bindings) env)))))

;; What `id` refers to in `env`: its local meaning; for an alias, its original
;; in the alias's environment; otherwise the global symbol itself.
(define %resolve
  (lambda (id env)
    (define meaning (%lookup id env))
    (if meaning
        meaning
        (if (%alias-original id)
            (%resolve (%alias-original id) (%alias-env id))
            id))))

;; The transformer `x` names, `x` a resolved identifier, or #f.
(define %macro
  (lambda (x)
    (%lookup-in x %macros '())))

;; What the head of `x` resolves to, if `x` is a pair headed by an identifier;
;; otherwise #f.
(define %keyword
  (lambda (x env)
    (if (pair? x)
        (if (symbol? (car x)) (%resolve (car x) env) #f)
        #f)))

;; `x` with macro uses at its head expanded, until its head isn't one.
(define %expand-head
  (lambda (x env)
    (define k (%keyword x env))
    (define transformer (if k (%macro k) #f))
    (if transformer (%expand-head (transformer x env) env) x)))

;; `x` with every alias replaced by its original.  Unchanged parts are shared.
(define %strip
  (lambda (x)
    (if (symbol? x)
        (if (%alias-original x) (%strip (%alias-original x)) x)
        (if (pair? x)
            (%strip-pair x (%strip (car x)) (%strip (cdr x)))
            (if (vector? x) (%strip-vector x) x)))))

(define %strip-pair
  (lambda (x a d)
    (if (if (eq? a (car x)) (eq? d (cdr x)) #f) x (cons a d))))

(define %strip-vector
  (lambda (v)
    (define lst (%vector->list v))
    (define stripped (%strip lst))
    (if (eq? stripped lst) v (apply vector stripped))))

(define %expand-list
  (lambda (lst env)
    (%map (lambda (x) (%expand-expr x env)) lst)))

;; Expands the expression `x` in `env`.
(define %expand-expr
  (lambda (x env)
    (define y (%expand-head x env))
    (define var (if (symbol? y) (%resolve y env) #f))
    (if var
        (if (%macro var) (%bad-syntax y) var)
        (if (pair? y) (%expand-form y env) (%strip y)))))

;; Expands the pair `x`, which isn't a macro use.
(define %expand-form
  (lambda (x env)
    (define k (%keyword x env))
    (define n (%length x 0))
    (if (not n)
        (%bad-syntax x)
        (if (eq? k 'quote)
            (if (= n 2) (list 'quote (%strip (car (cdr x)))) (%bad-syntax x))
            (if (eq? k 'quasiquote)
                (if (= n 2) (%qq (car (cdr x)) 0 env) (%bad-syntax x))
                (if (eq? k 'lambda)
                    (%expand-lambda x env)
                    (if (eq? k 'if)
                        (if (if (= n 3) #t (= n 4))
                            (cons 'if (%expand-list (cdr x) env))
                            (%bad-syntax x))
                        (if (eq? k 'set!)
                            (%expand-set x env)
                            (if (eq? k 'begin)
                                (cons 'begin (%expand-list (cdr x) env))
                                (if (eq? k 'define)
                                    (%bad-syntax x)  ; not at top level or the start of a body
                                    (%expand-list x env)))))))))))

(define %expand-set
  (lambda (x env)
    (define var
      (if (= (length x) 3)
          (if (symbol? (car (cdr x))) (%resolve (car (cdr x)) env) #f)
          #f))
    (if (if var (not (%macro var)) #f)
        (list 'set! var (%expand-expr (car (cdr (cdr x))) env))
        (%bad-syntax x))))

(define %expand-lambda
  (lambda (x env)
    (define inner (%extend env))
    (define rename
      (lambda (params)
        (if (pair? params)
            (if (symbol? (car params))
                (cons (%bind-variable! inner (car params)) (rename (cdr params)))
                (%bad-syntax x))
            (if (null? params)
                '()
                (if (symbol? params) (%bind-variable! inner params) (%bad-syntax x))))))
    (define params (if (< (length x) 3) (%bad-syntax x) (rename (car (cdr x)))))
    (cons 'lambda (cons params (%expand-body (cdr (cdr x)) inner x)))))

;; Expands the body `forms` of the form `whole` in a new frame over `env`
;; (R7RS 5.4).  The leading definitions, with `begin` spliced, are bound
;; first, so every definition and expression sees all of them.
(define %expand-body
  (lambda (forms env whole)
    (define inner (%extend env))
    ;; `defs` collects (name . expression) in reverse.
    (define scan
      (lambda (forms defs)
        (if (null? forms)
            (finish defs '())
            (scan-form (%expand-head (car forms) inner) (cdr forms) defs))))
    (define scan-form
      (lambda (x rest defs)
        (define k (%keyword x inner))
        (if (eq? k 'begin)
            (if (%length x 0) (scan (%append (cdr x) rest) defs) (%bad-syntax x))
            (if (eq? k 'define)
                (if (if (eq? (%length x 0) 3) (symbol? (car (cdr x))) #f)
                    (scan rest
                          (cons (cons (%bind-variable! inner (car (cdr x))) (car (cdr (cdr x))))
                                defs))
                    (%bad-syntax x))
                (finish defs (cons x rest))))))
    (define finish
      (lambda (defs exprs)
        (define body
          (%append (%map (lambda (d) (list 'define (car d) (%expand-expr (cdr d) inner)))
                         (%reverse defs))
                   (%expand-list exprs inner)))
        (if (null? body) (%bad-syntax whole) body)))
    (scan forms '())))

;; Code building the quasiquote template `t` at nesting depth `depth` (R7RS
;; 4.2.8), from cons, append, apply, vector, and quote.
(define %qq
  (lambda (t depth env)
    (define k (%keyword t env))
    (define keyword-then
      (lambda (depth)
        (%qq-cons (list 'quote (%strip (car t))) (%qq (cdr t) depth env))))
    (if (if (eq? k 'unquote) #t (eq? k 'unquote-splicing))
        (if (< 0 depth)
            (keyword-then (- depth 1))
            (if (if (eq? k 'unquote) (eq? (%length t 0) 2) #f)
                (%expand-expr (car (cdr t)) env)
                (%bad-syntax t)))
        (if (eq? k 'quasiquote)
            (keyword-then (+ depth 1))
            (if (pair? t)
                (if (if (= depth 0) (eq? (%keyword (car t) env) 'unquote-splicing) #f)
                    (if (eq? (%length (car t) 0) 2)
                        (list 'append (%expand-expr (car (cdr (car t))) env) (%qq (cdr t) depth env))
                        (%bad-syntax (car t)))
                    (%qq-cons (%qq (car t) depth env) (%qq (cdr t) depth env)))
                (if (vector? t)
                    (%qq-vector (%qq (%vector->list t) depth env))
                    (list 'quote (%strip t))))))))

(define %quoted?
  (lambda (code)
    (if (pair? code) (eq? (car code) 'quote) #f)))

(define %qq-cons
  (lambda (a d)
    (if (if (%quoted? a) (%quoted? d) #f)
        (list 'quote (cons (car (cdr a)) (car (cdr d))))
        (list 'cons a d))))

(define %qq-vector
  (lambda (code)
    (if (%quoted? code)
        (list 'quote (apply vector (car (cdr code))))
        (list 'apply 'vector code))))

;; Expands the top-level form `x`.
(define %expand
  (lambda (x)
    (define y (%expand-head x '()))
    (define k (%keyword y '()))
    (if (eq? k 'begin)
        (if (%length y 0) (cons 'begin (%map %expand (cdr y))) (%bad-syntax y))
        (if (eq? k 'define)
            (if (if (eq? (%length y 0) 3) (symbol? (car (cdr y))) #f)
                (list 'define (car (cdr y)) (%expand-expr (car (cdr (cdr y))) '()))
                (%bad-syntax y))
            (%expand-expr y '())))))

;;; List library.

;; Whether `x` is a proper list: finite and ending in '().  Floyd's cycle
;; detection makes cyclic lists return #f.
(define list?
  (lambda (x)
    (define loop
      (lambda (slow fast)
        (if (null? fast)
            #t
            (if (pair? fast)
                (if (null? (cdr fast))
                    #t
                    (if (pair? (cdr fast))
                        (if (eq? (cdr (cdr fast)) (cdr slow))
                            #f
                            (loop (cdr slow) (cdr (cdr fast))))
                        #f))
                #f))))
    (loop x x)))

(define list-tail
  (lambda (lst k)
    (if (= k 0)
        lst
        (list-tail (cdr lst) (- k 1)))))

(define list-ref
  (lambda (lst k)
    (car (list-tail lst k))))

;; Copies the pairs of `lst`; an improper tail is kept.
(define list-copy
  (lambda (lst)
    (if (pair? lst)
        (cons (car lst) (list-copy (cdr lst)))
        lst)))

(define reverse
  (lambda (lst)
    (define loop
      (lambda (lst acc)
        (if (null? lst)
            acc
            (loop (cdr lst) (cons (car lst) acc)))))
    (loop lst '())))

;; Every list but the last is copied; the last is shared.
(define append
  (lambda lists
    (define append2
      (lambda (a b)
        (if (null? a)
            b
            (cons (car a) (append2 (cdr a) b)))))
    (define loop
      (lambda (lists)
        (if (null? lists)
            '()
            (if (null? (cdr lists))
                (car lists)
                (append2 (car lists) (loop (cdr lists)))))))
    (loop lists)))

;; The first pair of `lst` whose car matches `x` under `same?`, or #f.
(define member
  (lambda (x lst . compare)
    (define same? (if (null? compare) equal? (car compare)))
    (define loop
      (lambda (lst)
        (if (null? lst)
            #f
            (if (same? x (car lst))
                lst
                (loop (cdr lst))))))
    (loop lst)))

(define memq (lambda (x lst) (member x lst eq?)))
(define memv (lambda (x lst) (member x lst eqv?)))

;; The first pair of `alist` whose car matches `x` under `same?`, or #f.
(define assoc
  (lambda (x alist . compare)
    (define same? (if (null? compare) equal? (car compare)))
    (define loop
      (lambda (alist)
        (if (null? alist)
            #f
            (if (same? x (car (car alist)))
                (car alist)
                (loop (cdr alist))))))
    (loop alist)))

(define assq (lambda (x alist) (assoc x alist eq?)))
(define assv (lambda (x alist) (assoc x alist eqv?)))

;; Applies `f` to the elements of the lists in turn, stopping at the shortest.
(define map
  (lambda (f lst . lists)
    (define map1
      (lambda (lst)
        (if (pair? lst)
            (cons (f (car lst)) (map1 (cdr lst)))
            '())))
    (define any-empty?
      (lambda (ls)
        (if (null? ls)
            #f
            (if (pair? (car ls))
                (any-empty? (cdr ls))
                #t))))
    (define cars
      (lambda (ls)
        (if (null? ls) '() (cons (car (car ls)) (cars (cdr ls))))))
    (define cdrs
      (lambda (ls)
        (if (null? ls) '() (cons (cdr (car ls)) (cdrs (cdr ls))))))
    (define mapn
      (lambda (ls)
        (if (any-empty? ls)
            '()
            (cons (apply f (cars ls)) (mapn (cdrs ls))))))
    (if (null? lists)
        (map1 lst)
        (mapn (cons lst lists)))))

;; Like map, but for effect; returns void.  With several lists, it uses map,
;; which also applies `f` from left to right.
(define for-each
  (lambda (f lst . lists)
    (define loop
      (lambda (lst)
        (if (pair? lst)
            (begin (f (car lst)) (loop (cdr lst)))
            (if #f #f))))
    (if (null? lists)
        (loop lst)
        (begin (apply map f lst lists) (if #f #f)))))

(define filter
  (lambda (keep? lst)
    (if (null? lst)
        '()
        (if (keep? (car lst))
            (cons (car lst) (filter keep? (cdr lst)))
            (filter keep? (cdr lst))))))

;; SRFI-1: (kons elem acc), from the left.
(define fold
  (lambda (kons knil lst)
    (if (null? lst)
        knil
        (fold kons (kons (car lst) knil) (cdr lst)))))

;; SRFI-1: (kons elem acc), from the right.
(define fold-right
  (lambda (kons knil lst)
    (if (null? lst)
        knil
        (kons (car lst) (fold-right kons knil (cdr lst))))))

;;; The expander and the derived forms, in Scheme.  Startup loads this file
;;; expanded (src/prelude.boot), so it may use the derived forms, and after
;;; src/prelude.scm, so it may use the list library.  Its forms load in order,
;;; and a define-syntax builds its transformer as it loads, so the code that
;;; runs then may call only what is defined above it or in src/prelude.scm.

;;; Expander.  (%expand form) rewrites a top-level form into core forms.  The
;;; evaluator applies it to every top-level form once %expand is defined.

(define (%bad-syntax x)
  (%syntax-error "bad syntax" (list x)))

;; A top-level environment is `#(kind bindings parent own)`.  `bindings` is an
;; association list of (identifier . binding), a binding being a label, the
;; global whose value slot is the variable, or a macro's transformer.  An
;; identifier it doesn't bind means what it does in `parent`, or else, with no
;; parent, is its own label, or in a library a fresh one, never bound.  `own`
;; lists the labels its definitions made.  `kind` is `system`, for the prelude
;; and expander; `interaction`, for the REPL and files, whose parent is the
;; system's; or `library`, with no parent.
(define (%make-top kind parent)
  (vector kind '() parent '()))

(define %system-env (%make-top 'system #f))
(define %interaction-env (%make-top 'interaction %system-env))

(define (%append-map f lst)
  (apply append (map f lst)))

;; Number of pairs in the chain `x`.
(define (%pairs x)
  (if (pair? x) (+ 1 (%pairs (cdr x))) 0))

(define (%take lst n)
  (if (= n 0) '() (cons (car lst) (%take (cdr lst) (- n 1)))))

;; Length of `x` plus `n`, or #f if `x` isn't a proper list.
(define (%length x n)
  (cond ((null? x) n)
        ((pair? x) (%length (cdr x) (+ n 1)))
        (else #f)))

;; Whether `x` is `(k identifier expression)`, as in define, define-syntax, set!.
(define (%binding-form? x)
  (and (eq? (%length x 0) 3) (symbol? (cadr x))))

;; The `define` form `x`, with `(define (name . formals) body ...)` rewritten as
;; `(define name (lambda formals body ...))`.  The `lambda` is an alias for the
;; special form, so a local binding can't capture it.
(define (%define-procedure x)
  (define n (%length x 0))
  (cond ((not (and (pair? (cdr x)) (pair? (cadr x)))) x)
        ((and n (< 2 n) (symbol? (car (cadr x))))
         `(,(car x) ,(car (cadr x))
           (,(%make-alias 'lambda %system-env) ,(cdr (cadr x)) ,@(cddr x))))
        (else (%bad-syntax x))))

;; A syntactic environment is a list of frames, innermost first, ending in a
;; top-level environment.  A frame is a one-element list holding an association
;; list of (identifier . meaning): a variable's fresh name, or a local macro's
;; transformer.

(define (%extend env)
  (cons (list '()) env))

;; Binds the identifier `id` in the innermost frame of `env` to a fresh name,
;; which it returns.
(define (%bind-variable! env id)
  (let ((name (gensym (symbol->string id))))
    (set-car! (car env) (cons (cons id name) (caar env)))
    name))

;; Binds the identifier `id` in the innermost frame of `env` to `transformer`.
(define (%bind-macro! env id transformer)
  (set-car! (car env) (cons (cons id transformer) (caar env))))

;; Meaning of `id` in `env`, or #f.
(define (%lookup id env)
  (and (pair? env) (%lookup-in id (caar env) (cdr env))))

;; Meaning of `id` in the association list `bindings`, or else in `env`.
(define (%lookup-in id bindings env)
  (cond ((assq id bindings) => cdr)
        (else (%lookup id env))))

;; The top-level environment `env` ends in.
(define (%top env)
  (if (pair? env) (%top (cdr env)) env))

;; The binding of `id` in the top-level environment `top` or its parents, or #f.
(define (%top-lookup id top)
  (and top
       (cond ((assq id (vector-ref top 1)) => cdr)
             (else (%top-lookup id (vector-ref top 2))))))

;; Binds `id` to `binding` in the top-level environment `top`, replacing any
;; binding it had there.  The system's names are their own labels unbound.
(define (%top-bind! top id binding)
  (let ((rest (filter (lambda (b) (not (eq? (car b) id))) (vector-ref top 1))))
    (vector-set! top 1 (if (and (eq? (vector-ref top 0) 'system) (eq? binding id))
                           rest
                           (cons (cons id binding) rest)))))

;; What `id` means in `env`: its local binding; else its top-level binding;
;; else, for an alias, its original's meaning in the alias's environment; else
;; its own label.
(define (%resolve id env)
  (cond ((%lookup id env))
        ((%top-lookup id (%top env)))
        ((%alias-original id) (%resolve (%alias-original id) (%alias-env id)))
        ((eq? (vector-ref (%top env) 0) 'library) (gensym (symbol->string id)))
        (else id)))

;; The transformer `x` is, if `x` is a resolved identifier bound to a macro, or #f.
(define (%macro x)
  (and (procedure? x) x))

;; What the head of `x` resolves to, if `x` is a pair headed by an identifier;
;; otherwise #f.
(define (%keyword x env)
  (and (pair? x) (symbol? (car x)) (%resolve (car x) env)))

;; `x` with macro uses at its head expanded, until its head isn't one.
(define (%expand-head x env)
  (let* ((k (%keyword x env))
         (transformer (and k (%macro k))))
    (if transformer (%expand-head (transformer x env) env) x)))

;; `x` with every alias replaced by its original.  Unchanged parts are shared.
(define (%strip x)
  (cond ((symbol? x) (if (%alias-original x) (%strip (%alias-original x)) x))
        ((pair? x)
         (let* ((a (%strip (car x)))
                (d (%strip (cdr x))))
           (if (and (eq? a (car x)) (eq? d (cdr x))) x (cons a d))))
        ((vector? x)
         (let* ((lst (vector->list x))
                (stripped (%strip lst)))
           (if (eq? stripped lst) x (list->vector stripped))))
        (else x)))

(define (%expand-list lst env)
  (map (lambda (x) (%expand-expr x env)) lst))

;; Expands the expression `x` in `env`.
(define (%expand-expr x env)
  (let* ((y (%expand-head x env))
         (var (and (symbol? y) (%resolve y env))))
    (cond (var (if (%macro var) (%bad-syntax y) var))
          ((pair? y) (%expand-form y env))
          (else (%strip y)))))

;; Expands the pair `x`, which isn't a macro use.
(define (%expand-form x env)
  (let* ((k (%keyword x env))
         (n (%length x 0)))
    (if (not n)
        (%bad-syntax x)
        (case k
          ((quote) (if (= n 2) (list 'quote (%strip (cadr x))) (%bad-syntax x)))
          ((quasiquote) (if (= n 2) (%qq (cadr x) 0 env) (%bad-syntax x)))
          ((lambda) (%expand-lambda x n env))
          ((if)
           (if (or (= n 3) (= n 4)) (cons 'if (%expand-list (cdr x) env)) (%bad-syntax x)))
          ((set!) (%expand-set x env))
          ((begin) (cons 'begin (%expand-list (cdr x) env)))
          ((define define-values define-syntax)
           (%bad-syntax x))  ; not at top level or the start of a body
          ((syntax-error)
           (if (<= 2 n) (%syntax-error (%strip (cadr x)) (%strip (cddr x))) (%bad-syntax x)))
          ((let-syntax) (%expand-let-syntax x env #f))
          ((letrec-syntax) (%expand-let-syntax x env #t))
          ((let-values) (%expand-let-values x env #f))
          ((letrec-values) (%expand-let-values x env #t))
          (else (%expand-list x env))))))

(define (%expand-set x env)
  (let ((var (and (%binding-form? x) (%resolve (cadr x) env))))
    (cond ((or (not var) (%macro var)) (%bad-syntax x))
          ((%imported? var (%top env)) (%syntax-error "set!: immutable variable" (list (cadr x))))
          (else (list 'set! var (%expand-expr (car (cddr x)) env))))))

;; The lambda list `formals` of the form `whole`, with each identifier `id`
;; replaced by `(f id)`.
(define (%map-formals f formals whole)
  (cond ((pair? formals)
         (if (symbol? (car formals))
             (cons (f (car formals)) (%map-formals f (cdr formals) whole))
             (%bad-syntax whole)))
        ((null? formals) '())
        ((symbol? formals) (f formals))
        (else (%bad-syntax whole))))

;; `formals` of the form `whole`, each identifier bound to a fresh name in `env`.
(define (%bind-formals! env formals whole)
  (%map-formals (lambda (id) (%bind-variable! env id)) formals whole))

(define (%expand-lambda x n env)
  (let* ((inner (%extend env))
         (params (if (< n 3) (%bad-syntax x) (%bind-formals! inner (cadr x) x))))
    `(lambda ,params ,@(%expand-body (cddr x) inner x))))

;; Expands `(let-values ((formals init) ...) body ...)`, or `letrec-values` if
;; `rec`.  The inits are expanded in `env`, or in the new frame if `rec`.
(define (%expand-let-values x env rec)
  (let* ((inner (%extend env))
         (clauses (if (and (< 2 (%length x 0)) (%length (cadr x) 0))
                      (map (lambda (c) (if (eq? (%length c 0) 2) c (%bad-syntax x))) (cadr x))
                      (%bad-syntax x)))
         (formals (map (lambda (c) (%bind-formals! inner (car c) x)) clauses))
         (inits (map (lambda (c) (%expand-expr (cadr c) (if rec inner env))) clauses)))
    `(,(if rec 'letrec-values 'let-values)
      ,(map list formals inits)
      ,@(%expand-body (cddr x) inner x))))

;; Expands the body `forms` of the form `whole` in a new frame over `env`
;; (R7RS 5.4).  The leading definitions, with `begin` spliced, are bound
;; first, so every definition and expression sees all of them, and become one
;; `letrec-values`.
(define (%expand-body forms env whole)
  (define inner (%extend env))
  ;; `defs` collects (formals . expression), names renamed and expressions
  ;; unexpanded, in reverse.
  (define (scan forms defs)
    (if (null? forms)
        (finish defs '())
        (scan-form (%expand-head (car forms) inner) (cdr forms) defs)))
  (define (scan-form form rest defs)
    (let* ((k (%keyword form inner))
           (x (if (eq? k 'define) (%define-procedure form) form)))
      (case k
        ((begin)
         (if (%length x 0) (scan (append (cdr x) rest) defs) (%bad-syntax x)))
        ((define)
         (if (%binding-form? x)
             (scan rest (cons (cons (list (%bind-variable! inner (cadr x))) (car (cddr x))) defs))
             (%bad-syntax x)))
        ((define-values)
         (if (eq? (%length x 0) 3)
             (scan rest (cons (cons (%bind-formals! inner (cadr x) x) (car (cddr x))) defs))
             (%bad-syntax x)))
        ((define-syntax)
         (if (%binding-form? x)
             (begin
               (%bind-macro! inner (cadr x) (%transformer (car (cddr x)) inner))
               (scan rest defs))
             (%bad-syntax x)))
        (else (finish defs (cons x rest))))))
  (define (finish defs exprs)
    (let* ((clauses (map (lambda (d) (list (car d) (%expand-expr (cdr d) inner)))
                         (reverse defs)))
           (body (if (null? exprs) (%bad-syntax whole) (%expand-list exprs inner))))
      (if (null? clauses) body `((letrec-values ,clauses ,@body)))))
  (scan forms '()))

;; Expands `(let-syntax ((k spec) ...) body ...)`, or `letrec-syntax` if
;; `rec`, into `(let-values () body ...)`.  The transformers close over `env`,
;; or over the new frame if `rec`.
(define (%expand-let-syntax x env rec)
  (let ((inner (%extend env)))
    (if (and (< 2 (%length x 0)) (%length (cadr x) 0))
        (for-each (lambda (b)
                    (if (and (eq? (%length b 0) 2) (symbol? (car b)))
                        (%bind-macro! inner (car b) (%transformer (cadr b) (if rec inner env)))
                        (%bad-syntax x)))
                  (cadr x))
        (%bad-syntax x))
    `(let-values () ,@(%expand-body (cddr x) inner x))))

;; Code building the quasiquote template `t` at nesting depth `depth` (R7RS
;; 4.2.8), from cons, append, list->vector, and quote.
(define (%qq t depth env)
  (let ((k (%keyword t env)))
    (define (keyword-then depth)
      (%qq-cons (list 'quote (%strip (car t))) (%qq (cdr t) depth env)))
    (cond ((memq k '(unquote unquote-splicing))
           (cond ((< 0 depth) (keyword-then (- depth 1)))
                 ((and (eq? k 'unquote) (eq? (%length t 0) 2)) (%expand-expr (cadr t) env))
                 (else (%bad-syntax t))))
          ((eq? k 'quasiquote) (keyword-then (+ depth 1)))
          ((and (pair? t) (= depth 0) (eq? (%keyword (car t) env) 'unquote-splicing))
           (if (eq? (%length (car t) 0) 2)
               (list 'append (%expand-expr (cadr (car t)) env) (%qq (cdr t) depth env))
               (%bad-syntax (car t))))
          ((pair? t) (%qq-cons (%qq (car t) depth env) (%qq (cdr t) depth env)))
          ((vector? t) (%qq-vector (%qq (vector->list t) depth env)))
          (else (list 'quote (%strip t))))))

(define (%quoted? code)
  (and (pair? code) (eq? (car code) 'quote)))

(define (%qq-cons a d)
  (if (and (%quoted? a) (%quoted? d))
      (list 'quote (cons (cadr a) (cadr d)))
      (list 'cons a d)))

(define (%qq-vector code)
  (if (%quoted? code)
      (list 'quote (list->vector (cadr code)))
      (list 'list->vector code)))

;; The transformer `spec` denotes in `env`: a procedure from a macro use and its
;; environment to the use's expansion.
(define (%transformer spec env)
  (if (eq? (%keyword spec env) 'syntax-rules)
      (%syntax-rules spec env)
      (%bad-syntax spec)))

;; The transformer for `(syntax-rules [ellipsis] (literal ...) rule ...)` in
;; `env` (R7RS 4.3.2).  A binding of a pattern variable is (identifier depth .
;; match), where a match at depth n > 0 is a list of matches at depth n - 1.
(define (%syntax-rules spec env)
  (define custom (and (pair? (cdr spec)) (symbol? (cadr spec))))
  (define rest (if custom (cddr spec) (cdr spec)))
  (define ellipsis (%resolve (if custom (cadr spec) '...) env))
  (define literals (if (pair? rest) (car rest) (%bad-syntax spec)))
  (define rules (cdr rest))
  (define renames '())  ; one alias per identifier per use

  (define (ellipsis? x) (and (symbol? x) (eq? (%resolve x env) ellipsis)))
  (define (literal? x) (memq x literals))
  (define (underscore? x) (eq? (%resolve x env) '_))
  (define (followed-by-ellipsis? x) (and (pair? (cdr x)) (ellipsis? (cadr x))))

  ;; The pattern variables of `pat` at nesting `depth`, as (identifier . depth).
  (define (pattern-vars pat depth acc)
    (cond ((symbol? pat)
           (if (or (literal? pat) (underscore? pat) (ellipsis? pat))
               acc
               (cons (cons pat depth) acc)))
          ((pair? pat)
           (if (followed-by-ellipsis? pat)
               (pattern-vars (cddr pat) depth (pattern-vars (car pat) (+ depth 1) acc))
               (pattern-vars (cdr pat) depth (pattern-vars (car pat) depth acc))))
          ((vector? pat) (pattern-vars (vector->list pat) depth acc))
          (else acc)))

  ;; `b` extended with the bindings that make `pat` match `form`, or #f.
  (define (match pat form use-env b)
    (cond ((not b) #f)
          ((symbol? pat)
           (cond ((literal? pat)
                  (and (symbol? form) (eq? (%resolve form use-env) (%resolve pat env)) b))
                 ((underscore? pat) b)
                 (else (cons (cons pat (cons 0 form)) b))))
          ((pair? pat)
           (cond ((followed-by-ellipsis? pat) (match-ellipsis pat form use-env b))
                 ((pair? form)
                  (match (cdr pat) (cdr form) use-env (match (car pat) (car form) use-env b)))
                 (else #f)))
          ((null? pat) (and (null? form) b))
          ((vector? pat)
           (and (vector? form) (match (vector->list pat) (vector->list form) use-env b)))
          (else (and (equal? pat form) b))))

  ;; Matches `(p ellipsis . rest)`: `p` against as many elements as `rest`
  ;; leaves, binding each variable of `p` to the list of its matches.
  (define (match-ellipsis pat form use-env b)
    (let* ((p (car pat))
           (rest (cddr pat))
           (n (- (%pairs form) (%pairs rest)))
           (matches (if (< n 0)
                        '(#f)
                        (map (lambda (x) (match p x use-env '())) (%take form n)))))
      (and (not (memq #f matches))
           (match rest (list-tail form n) use-env
                  (bind-sequences (pattern-vars p 0 '()) matches b)))))

  (define (bind-sequences vars matches b)
    (if (null? vars)
        b
        (let ((v (car vars)))
          (bind-sequences (cdr vars)
                          matches
                          (cons (cons (car v)
                                      (cons (+ (cdr v) 1)
                                            (map (lambda (m) (cddr (assq (car v) m))) matches)))
                                b)))))

  ;; The alias for the template identifier `id` in this use.
  (define (alias id)
    (cond ((assq id renames) => cdr)
          (else (let ((new (%make-alias id env)))
                  (set! renames (cons (cons id new) renames))
                  new))))

  ;; Instantiates `tmpl` with the bindings `b`; `escaped` disables the ellipsis.
  (define (instantiate tmpl b escaped)
    (cond ((symbol? tmpl)
           (let ((binding (assq tmpl b)))
             (cond ((not binding) (alias tmpl))
                   ((= (cadr binding) 0) (cddr binding))
                   (else (%bad-syntax tmpl)))))
          ((pair? tmpl)
           (cond ((and (not escaped) (ellipsis? (car tmpl)) (pair? (cdr tmpl)))
                  (instantiate (cadr tmpl) b #t))  ; (... template)
                 ((and (not escaped) (followed-by-ellipsis? tmpl))
                  (instantiate-ellipses (car tmpl) (cdr tmpl) 0 b))
                 (else (cons (instantiate (car tmpl) b escaped)
                             (instantiate (cdr tmpl) b escaped)))))
          ((vector? tmpl) (list->vector (instantiate (vector->list tmpl) b escaped)))
          (else tmpl)))

  ;; Instantiates `e` followed by `k` ellipses so far and then `rest`.
  (define (instantiate-ellipses e rest k b)
    (if (and (pair? rest) (ellipsis? (car rest)))
        (instantiate-ellipses e (cdr rest) (+ k 1) b)
        (append (repeat e k b) (instantiate rest b #f))))

  ;; The instantiations of `e` under `k` ellipses, as a list.
  (define (repeat e k b)
    (if (= k 0)
        (list (instantiate e b #f))
        (%append-map (lambda (b) (repeat e (- k 1) b)) (iterations e b))))

  ;; One binding list per step of an ellipsis over `e`, each binding `e`'s
  ;; sequence variables to their next elements.
  (define (iterations e b)
    (let ((vars (sequence-vars e b '())))
      (define (step seqs)
        (cond ((null? (car seqs)) (if (null? (filter pair? seqs)) '() (%bad-syntax e)))
              ((memq '() seqs) (%bad-syntax e))
              (else (cons (bind-step vars seqs b) (step (map cdr seqs))))))
      (if (null? vars) (%bad-syntax e) (step (map cddr vars)))))

  ;; The bindings of `b` for identifiers in `tmpl` that are bound to sequences.
  (define (sequence-vars tmpl b acc)
    (cond ((symbol? tmpl)
           (let ((binding (assq tmpl b)))
             (if (and binding (< 0 (cadr binding))) (cons binding acc) acc)))
          ((pair? tmpl) (sequence-vars (cdr tmpl) b (sequence-vars (car tmpl) b acc)))
          ((vector? tmpl) (sequence-vars (vector->list tmpl) b acc))
          (else acc)))

  (define (bind-step vars seqs b)
    (if (null? vars)
        b
        (bind-step (cdr vars)
                   (cdr seqs)
                   (cons (cons (caar vars) (cons (- (cadr (car vars)) 1) (caar seqs))) b))))

  (define (apply-rules rules form use-env)
    (let* ((rule (if (null? rules) (%bad-syntax form) (car rules)))
           (b (if (and (eq? (%length rule 0) 2) (pair? (car rule)))
                  (match (cdar rule) (cdr form) use-env '())
                  (%bad-syntax spec))))
      (if b
          (instantiate (cadr rule) b #f)
          (apply-rules (cdr rules) form use-env))))

  (lambda (form use-env)
    (set! renames '())
    (apply-rules rules form use-env)))

;; Whether the label `label` is one the top-level environment `top` defined.
(define (%own-label? top label)
  (if (eq? (vector-ref top 0) 'system)
      (%bound? label)
      (memq label (vector-ref top 3))))

;; Whether the binding `b`, in the top-level environment `top`, is imported:
;; a system binding, outside the system's environment.
(define (%imported? b top)
  (and (not (eq? (vector-ref top 0) 'system))
       (not (%own-label? top b))
       (or (procedure? b) (%bound? b))))

;; The label a top-level definition of `id` in `top` defines.  It redefines
;; `top`'s own variable, and otherwise binds `id` to itself, or to a fresh label
;; in a library, if it would shadow an imported binding, or if `id` is an
;; alias, inserted by a macro, so only that use's insertions refer to it
;; (R7RS 5.3.1).
(define (%top-define! top id)
  (let ((b (%resolve id top)))
    (cond ((and (symbol? b) (%own-label? top b)) b)
          ((or (eq? (vector-ref top 0) 'library) (%alias-original id) (%imported? b top))
           (%top-own! top id (gensym (symbol->string id))))
          (else (%top-own! top id id)))))

;; Binds `id` to `label`, a label of its own, in `top`; returns `label`.
(define (%top-own! top id label)
  (%top-bind! top id label)
  (vector-set! top 3 (cons label (vector-ref top 3)))
  label)

(define (%expand-define form top)
  (let* ((x (%define-procedure form))
         (name (if (%binding-form? x) (%top-define! top (cadr x)) (%bad-syntax x))))
    (list 'define name (%expand-expr (car (cddr x)) top))))

(define (%expand-define-values x top)
  (let ((formals (if (eq? (%length x 0) 3)
                     (%map-formals (lambda (id) (%top-define! top id)) (cadr x) x)
                     (%bad-syntax x))))
    (list 'define-values formals (%expand-expr (car (cddr x)) top))))

;; Binds the identifier `id` to the macro `spec` denotes, in `top`.
(define (%top-define-macro! top id spec)
  (%top-bind! top id (%transformer spec top)))

;; Binds the system macro `name` to the transformer `spec` denotes.
(define (%define-macro! name spec)
  (%top-define-macro! %system-env name spec))

(define (%expand-define-syntax x top)
  (if (%binding-form? x)
      (%top-define-macro! top (cadr x) (car (cddr x)))
      (%bad-syntax x))
  '(begin))

;; Expands the top-level form `x` in the top-level environment `top`.
(define (%expand-top x top)
  (let* ((y (%expand-head x top))
         (k (%keyword y top)))
    (case k
      ((begin)
       (if (%length y 0)
           (cons 'begin (map (lambda (z) (%expand-top z top)) (cdr y)))
           (%bad-syntax y)))
      ((define) (%expand-define y top))
      ((define-values) (%expand-define-values y top))
      ((define-syntax) (%expand-define-syntax y top))
      ((import) (%import! top (cdr y)) '(begin))
      ((define-library) (%expand-define-library y))
      (else (%expand-expr y top)))))

;; Expands the top-level form `x` in the interaction environment.
(define (%expand x)
  (%expand-top x %interaction-env))

;;; Libraries (R7RS 5.6).

;; The libraries defined: (name . exports), the exports an association list of
;; (name . binding).
(define %libraries '())

;; Registers the library `name` with the exports `exports`, replacing any
;; library of that name.
(define (%define-library! name exports)
  (set! %libraries
        (cons (cons name exports)
              (filter (lambda (lib) (not (equal? (car lib) name))) %libraries))))

;; Defines the library `name`, exporting the system's bindings of `names`.
(define (%define-system-library name names)
  (%define-library! name (map (lambda (n) (cons n (%resolve n %system-env))) names)))

;; The directory of the library file being loaded, for `include`, or "".
(define %library-dir "")

;; `s` split at each `sep`.
(define (%split s sep)
  (let loop ((cs (string->list s)) (part '()) (parts '()))
    (cond ((null? cs) (reverse (cons (list->string (reverse part)) parts)))
          ((char=? (car cs) sep) (loop (cdr cs) '() (cons (list->string (reverse part)) parts)))
          (else (loop (cdr cs) (cons (car cs) part) parts)))))

;; The path `file` in the directory `dir`, "" being the current one.
(define (%join dir file)
  (if (equal? dir "") file (string-append dir "/" file)))

;; The directory part of `path`, or "".
(define (%path-dir path)
  (let loop ((cs (reverse (string->list path))))
    (cond ((null? cs) "")
          ((char=? (car cs) #\/) (list->string (reverse (cdr cs))))
          (else (loop (cdr cs))))))

;; The file for the library `name`, relative to a directory: `(foo bar)` is in
;; `foo/bar.sld`.
(define (%library-file name)
  (define (part x) (if (symbol? x) (symbol->string x) (number->string x)))
  (let loop ((parts (cdr name)) (path (part (car name))))
    (if (null? parts)
        (string-append path ".sld")
        (loop (cdr parts) (string-append path "/" (part (car parts)))))))

;; Loads the file for the library `name` from the first directory that has
;; it: the current one, then those `MINIM_LIBRARY_PATH` lists, colon-separated.
(define (%load-library name)
  (let ((file (%library-file name))
        (var (get-environment-variable "MINIM_LIBRARY_PATH")))
    (let loop ((dirs (cons "" (if var (%split var #\:) '()))))
      (unless (null? dirs)
        (let ((path (%join (car dirs) file))
              (outer %library-dir))
          (if (file-exists? path)
              (dynamic-wind (lambda () (set! %library-dir (%path-dir path)))
                            (lambda () (load path))
                            (lambda () (set! %library-dir outer)))
              (loop (cdr dirs))))))))

;; The exports of the library `name`, loading its file if it isn't defined.
(define (%library-exports name)
  (unless (assoc name %libraries)
    (%load-library name))
  (cond ((assoc name %libraries) => cdr)
        (else (%syntax-error "import: unknown library" (list name)))))

;; The forms of the files `files`, relative to the library file's directory.
(define (%include files)
  (%append-map (lambda (file)
                 (let ((path (if (string? file) (%join %library-dir file) (%bad-syntax file))))
                   (if (file-exists? path)
                       (%read-file path)
                       (%syntax-error "include: cannot open file" (list path)))))
               files))

;; The (name . binding) pairs the stripped import set `set` names.
(define (%import-set set)
  (define (names-in bindings names)
    (for-each (lambda (n)
                (unless (assq n bindings) (%syntax-error "import: not exported" (list n set))))
              names)
    names)
  (define (inner)
    (if (and (eq? (%length set 0) 3) (symbol? (car (cddr set))))
        (%import-set (cadr set))
        (%bad-syntax set)))
  (case (and (pair? set) (car set))
    ((only)
     (let* ((bs (%import-set (cadr set)))
            (names (names-in bs (cddr set))))
       (filter (lambda (b) (memq (car b) names)) bs)))
    ((except)
     (let* ((bs (%import-set (cadr set)))
            (names (names-in bs (cddr set))))
       (filter (lambda (b) (not (memq (car b) names))) bs)))
    ((prefix)
     (let ((p (symbol->string (car (cddr set)))))
       (map (lambda (b) (cons (string->symbol (string-append p (symbol->string (car b)))) (cdr b)))
            (inner))))
    ((rename)
     (let* ((bs (%import-set (cadr set)))
            (renames (cddr set)))
       (names-in bs (map car renames))
       (map (lambda (b) (cond ((assq (car b) renames) => (lambda (r) (cons (cadr r) (cdr b))))
                              (else b)))
            bs)))
    (else (if (%length set 0) (%library-exports set) (%bad-syntax set)))))

;; Binds, in `top`, the names the import sets `sets` name.
(define (%import! top sets)
  (for-each (lambda (set)
              (for-each (lambda (b) (%top-bind! top (car b) (cdr b))) (%import-set (%strip set))))
            sets))

;; Expands the body of a library, the forms `forms`, in its environment `top`:
;; every definition is bound before any form is expanded, so one may refer to
;; one further down.  The definitions and expressions run in order.
(define (%expand-library-body forms top)
  ;; `items` collects, in reverse, a procedure expanding each form.
  (define (scan forms items)
    (if (null? forms)
        (reverse items)
        (let* ((x (%expand-head (car forms) top))
               (k (%keyword x top))
               (rest (cdr forms)))
          (case k
            ((begin) (if (%length x 0) (scan (append (cdr x) rest) items) (%bad-syntax x)))
            ((define)
             (let ((d (%define-procedure x)))
               (if (%binding-form? d)
                   (let ((name (%top-define! top (cadr d))))
                     (scan rest (cons (lambda ()
                                        (list 'define name (%expand-expr (car (cddr d)) top)))
                                      items)))
                   (%bad-syntax x))))
            ((define-values)
             (if (eq? (%length x 0) 3)
                 (let ((formals (%map-formals (lambda (id) (%top-define! top id)) (cadr x) x)))
                   (scan rest (cons (lambda () (list 'define-values formals
                                                     (%expand-expr (car (cddr x)) top)))
                                    items)))
                 (%bad-syntax x)))
            ((define-syntax) (%expand-define-syntax x top) (scan rest items))
            (else (scan rest (cons (lambda () (%expand-expr x top)) items)))))))
  (cons 'begin (map (lambda (item) (item)) (scan forms '()))))

;; The (external name . binding) an export spec of the library environment `top`
;; denotes: `name`, or `(rename name external)`.
(define (%export spec top)
  (let* ((renamed (and (pair? spec) (eq? (car spec) 'rename) (eq? (%length spec 0) 3)))
         (name (if renamed (cadr spec) spec))
         (b (and (symbol? name) (%top-lookup name top))))
    (cons (if renamed (car (cddr spec)) name)
          (or b (%syntax-error "define-library: not defined" (list name))))))

;; Expands `(define-library name decl ...)`: registers the library, and expands
;; to its body, which runs once, when the form does.
(define (%expand-define-library form)
  (let ((x (%strip form))
        (top (%make-top 'library #f)))
    (unless (and (%length x 0) (<= 2 (%length x 0)) (%length (cadr x) 0)) (%bad-syntax x))
    (let loop ((decls (cddr x)) (specs '()) (body '()))
      (if (null? decls)
          (let ((code (%expand-library-body body top)))
            (%define-library! (cadr x) (map (lambda (s) (%export s top)) specs))
            code)
          (let ((d (car decls)))
            (case (and (pair? d) (%length d 0) (car d))
              ((export) (loop (cdr decls) (append specs (cdr d)) body))
              ((import) (%import! top (cdr d)) (loop (cdr decls) specs body))
              ((begin) (loop (cdr decls) specs (append body (cdr d))))
              ((include) (loop (cdr decls) specs (append body (%include (cdr d)))))
              (else (%bad-syntax d))))))))

;;; Derived forms (R7RS 4.2), as macros.

(define-syntax let
  (syntax-rules ()
    ((_ ((name val) ...) body1 body2 ...)
     (let-values (((name) val) ...) body1 body2 ...))
    ((_ tag ((name val) ...) body1 body2 ...)
     ((letrec ((tag (lambda (name ...) body1 body2 ...))) tag) val ...))))

(define-syntax let*
  (syntax-rules ()
    ((_ () body1 body2 ...) (let () body1 body2 ...))
    ((_ ((name val) rest ...) body1 body2 ...)
     (let ((name val)) (let* (rest ...) body1 body2 ...)))))

(define-syntax let*-values
  (syntax-rules ()
    ((_ () body1 body2 ...) (let () body1 body2 ...))
    ((_ (binding rest ...) body1 body2 ...)
     (let-values (binding) (let*-values (rest ...) body1 body2 ...)))))

;; letrec-values evaluates each `val` in order, seeing every name.
(define-syntax letrec*
  (syntax-rules ()
    ((_ ((name val) ...) body1 body2 ...)
     (letrec-values (((name) val) ...) body1 body2 ...))))

(define-syntax letrec
  (syntax-rules ()
    ((_ bindings body1 body2 ...) (letrec* bindings body1 body2 ...))))

(define-syntax and
  (syntax-rules ()
    ((_) #t)
    ((_ test) test)
    ((_ test1 test2 ...) (if test1 (and test2 ...) #f))))

(define-syntax or
  (syntax-rules ()
    ((_) #f)
    ((_ test) test)
    ((_ test1 test2 ...) (let ((x test1)) (if x x (or test2 ...))))))

(define-syntax when
  (syntax-rules ()
    ((_ test result1 result2 ...) (if test (begin result1 result2 ...)))))

(define-syntax unless
  (syntax-rules ()
    ((_ test result1 result2 ...) (if test (if #f #f) (begin result1 result2 ...)))))

(define-syntax cond
  (syntax-rules (else =>)
    ((_ (else result1 result2 ...)) (begin result1 result2 ...))
    ((_ (test => result)) (let ((temp test)) (if temp (result temp))))
    ((_ (test => result) clause1 clause2 ...)
     (let ((temp test)) (if temp (result temp) (cond clause1 clause2 ...))))
    ((_ (test)) test)
    ((_ (test) clause1 clause2 ...)
     (let ((temp test)) (if temp temp (cond clause1 clause2 ...))))
    ((_ (test result1 result2 ...)) (if test (begin result1 result2 ...)))
    ((_ (test result1 result2 ...) clause1 clause2 ...)
     (if test (begin result1 result2 ...) (cond clause1 clause2 ...)))))

(define-syntax case
  (syntax-rules (else =>)
    ((_ (key ...) clauses ...)
     (let ((atom-key (key ...))) (case atom-key clauses ...)))
    ((_ key (else => result)) (result key))
    ((_ key (else result1 result2 ...)) (begin result1 result2 ...))
    ((_ key ((atoms ...) => result)) (if (memv key '(atoms ...)) (result key)))
    ((_ key ((atoms ...) => result) clause clauses ...)
     (if (memv key '(atoms ...)) (result key) (case key clause clauses ...)))
    ((_ key ((atoms ...) result1 result2 ...))
     (if (memv key '(atoms ...)) (begin result1 result2 ...)))
    ((_ key ((atoms ...) result1 result2 ...) clause clauses ...)
     (if (memv key '(atoms ...)) (begin result1 result2 ...) (case key clause clauses ...)))))

(define-syntax do
  (syntax-rules ()
    ((_ ((var init step ...) ...) (test expr ...) command ...)
     (let loop ((var init) ...)
       (if test
           (begin (if #f #f) expr ...)
           (begin command ... (loop (do "step" var step ...) ...)))))
    ((_ "step" x) x)
    ((_ "step" x y) y)))

(define-syntax guard
  (syntax-rules (else)
    ((_ (var clause ... (else result1 result2 ...)) body1 body2 ...)
     (%guard (lambda () body1 body2 ...)
             (lambda (var reraise) (cond clause ... (else result1 result2 ...)))))
    ((_ (var clause ...) body1 body2 ...)
     (%guard (lambda () body1 body2 ...)
             (lambda (var reraise) (cond clause ... (else (reraise))))))))

(define-syntax define-record-type
  (syntax-rules ()
    ((_ type (ctor cfield ...) pred (field accessor . modifier) ...)
     (begin
       (define type (%make-record-type 'type '(field ...)))
       (define ctor
         (let ((make (%record-constructor type '(cfield ...))))
           (lambda (cfield ...) (make (list cfield ...)))))
       (define pred (%record-predicate type))
       (%define-record-field type field accessor . modifier) ...))))

(define-syntax %define-record-field
  (syntax-rules ()
    ((_ type field accessor)
     (define accessor (%record-accessor type 'field 'accessor)))
    ((_ type field accessor modifier)
     (begin
       (define accessor (%record-accessor type 'field 'accessor))
       (define modifier (%record-modifier type 'field 'modifier))))))

;;; Standard libraries (R7RS 5.6.1, Appendix A): the names Minim has.

(%define-system-library
 '(scheme base)
 '(* + - ... < <= = => > >= _ and append apply assoc assq assv begin boolean? caar cadr
   call-with-current-continuation call-with-values call/cc car case cdar cddr cdr
   char->integer char<=? char<? char=? char>=? char>? char? cond cons define
   define-record-type define-syntax define-values do dynamic-wind else eq? equal? eqv?
   error error-object-irritants error-object-message error-object? file-error? for-each
   guard if integer->char lambda length let let* let*-values let-syntax let-values letrec
   letrec* letrec-syntax list list->string list->vector list-copy list-ref list-tail list?
   make-string make-vector map member memq memv newline not null? number->string or pair?
   procedure? quasiquote quote quotient raise raise-continuable read-error? remainder
   reverse set! set-car! set-cdr! string->list string->symbol string-append string-length
   string-ref string-set! string<=? string<? string=? string>=? string>? string? substring
   symbol->string symbol? syntax-error syntax-rules unless unquote unquote-splicing values
   vector vector->list vector-length vector-ref vector-set! vector? when
   with-exception-handler zero?))

(%define-system-library '(scheme file) '(file-exists?))
(%define-system-library '(scheme load) '(load))
(%define-system-library '(scheme process-context) '(get-environment-variable))
(%define-system-library '(scheme write) '(display write))

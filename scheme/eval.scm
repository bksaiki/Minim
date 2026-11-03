;;; Eval (R7RS 6.12): environment specifiers, `environment`,
;;; `interaction-environment`, and `eval`; and `%expand`, which the evaluator
;;; applies to every top-level form.

;; An environment specifier: a top-level environment, out of user code's reach,
;; and whether definitions may add to it.
(define-record-type <environment>
  (%make-environment top mutable?)
  %environment?
  (top %environment-top)
  (mutable? %environment-mutable?))

;; The specifiers `environment` made, by their import sets: their bindings are
;; immutable, so the same sets share one.
(define %environments '())

;; A copy of `x`, so mutating the caller's import sets can't change a key.
(define (%copy-sets x)
  (if (pair? x) (cons (%copy-sets (car x)) (%copy-sets (cdr x))) x))

(define (environment . sets)
  (cond ((assoc sets %environments) => cdr)
        (else
         (let ((top (%make-top 'library #f)))
           (%import! top sets)
           (let ((env (%make-environment top #f)))
             (set! %environments (cons (cons (%copy-sets sets) env) %environments))
             env)))))

;; The environment the REPL, `load`, and minim_define expand in: a parameter,
;; as in Chez and Chibi, initially the REPL's own.
(define interaction-environment
  (make-parameter (%make-environment %interaction-env #t)
                  (lambda (env)
                    (unless (%environment? env)
                      (error "interaction-environment: not an environment" env))
                    env)))

;; Expands `x` in `env`: as a top-level form if `env` is mutable, else as an
;; expression, so a definition is bad syntax.
(define (%expand-in x env)
  (let ((top (%environment-top env)))
    (if (%environment-mutable? env) (%expand-top x top) (%expand-expr x top))))

(define (eval x env)
  (unless (%environment? env)
    (error "eval: not an environment" env))
  (%eval-expanded (%expand-in x env)))

;; Expands the top-level form `x` in the interaction environment.
(define (%expand x)
  (%expand-in x (interaction-environment)))

;; The label a top-level definition of `id` in the interaction environment
;; defines; for minim_define.
(define (%interaction-label! id)
  (let ((env (interaction-environment)))
    (unless (%environment-mutable? env)
      (error "minim_define: immutable interaction environment" id))
    (%top-define! (%environment-top env) id)))

;; R5RS's environments, for `version` 5 only.
(define (scheme-report-environment version)
  (unless (eqv? version 5)
    (error "scheme-report-environment: not version 5" version))
  (environment '(scheme r5rs)))

;; Only (scheme r5rs)'s syntax: its exports bound to macros, or to labels
;; holding no value, the core forms and auxiliary syntax.
(define (null-environment version)
  (unless (eqv? version 5)
    (error "null-environment: not version 5" version))
  (environment
   (append '(only (scheme r5rs))
           (map car (filter (lambda (b) (or (procedure? (cdr b)) (not (%bound? (cdr b)))))
                            (%library-exports '(scheme r5rs)))))))

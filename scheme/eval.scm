;;; Eval (R7RS 6.12): environment specifiers, `environment`,
;;; `interaction-environment`, and `eval`.

;; An environment specifier: a top-level environment, out of user code's reach.
(define-record-type <environment>
  (%make-environment top)
  %environment?
  (top %environment-top))

;; The specifiers `environment` made, by their import sets: their bindings are
;; immutable, so the same sets share one.
(define %environments '())

(define (environment . sets)
  (cond ((assoc sets %environments) => cdr)
        (else
         (let ((top (%make-top 'library #f)))
           (%import! top sets)
           (let ((env (%make-environment top)))
             (set! %environments (cons (cons sets env) %environments))
             env)))))

(define %interaction-environment (%make-environment %interaction-env))

(define (interaction-environment)
  %interaction-environment)

;; Expands `x` in `env` and evaluates it: in the interaction environment as a
;; top-level form, elsewhere as an expression, so a definition is bad syntax.
(define (eval x env)
  (unless (%environment? env)
    (error "eval: not an environment" env))
  (let ((top (%environment-top env)))
    (%eval-expanded (if (eq? top %interaction-env)
                        (%expand-top x top)
                        (%expand-expr x top)))))

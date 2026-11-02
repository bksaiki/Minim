;;; Expands the Scheme sources' top-level forms for scheme/prelude.boot
;;; (tools/boot_gen.c).

(import (minim system))  ; the expander's internals

;; The core forms the top-level form `x` expands to, in order, each fresh name
;; replaced by an interned one unique within `x`.
(define %boot-form
  (lambda (x)
    (let ((count (%boot-labels)))
      (let ((forms (%boot-expand x)))
        (if (= (%boot-labels) count)
            (%boot-names forms)
            (%syntax-error "boot: a macro defined a fresh global" (list x)))))))

;; Number of identifiers the system environment binds to labels other than
;; themselves, which only fresh globals add.
(define %boot-labels
  (lambda ()
    (length (filter (lambda (id)
                      (let ((b (%getprop id %system-env #f)))
                        (and (symbol? b) (not (eq? b id)))))
                    (vector-ref %system-env 3)))))

;; The core forms the top-level form `x` expands to: a `begin` spliced, and a
;; `define-syntax` registered and written as a call that rebuilds the macro.
(define %boot-expand
  (lambda (x)
    (let* ((y (%expand-head x %system-env))
           (k (%keyword y %system-env)))
      (cond ((eq? k 'begin)
             (if (%length y 0) (%append-map %boot-expand (cdr y)) (%bad-syntax y)))
            ((eq? k 'define-syntax)
             (%expand-top y %system-env)
             (list (list '%define-macro!
                         (list 'quote (car (cdr y)))
                         (list 'quote (%strip (car (cdr (cdr y))))))))
            (else (list (%expand-top y %system-env)))))))

;; `x` with each uninterned symbol replaced by the interned `name.N`, where N
;; counts the distinct symbols named `name` in order of appearance.
(define %boot-names
  (lambda (x)
    (define renames '())  ; (uninterned symbol . new name)
    (define counts '())   ; (name . symbols so far)
    (define rename
      (lambda (s)
        (let ((name (symbol->string s)))
          (cond ((eq? s (string->symbol name)) s)
                ((assq s renames) => cdr)
                (else
                 (let* ((count (assoc name counts))
                        (n (if count (+ (cdr count) 1) 1))
                        (new (string->symbol (string-append name "." (number->string n)))))
                   (if count (set-cdr! count n) (set! counts (cons (cons name n) counts)))
                   (set! renames (cons (cons s new) renames))
                   new))))))
    (define walk
      (lambda (x)
        (cond ((symbol? x) (rename x))
              ((pair? x) (let* ((a (walk (car x))) (d (walk (cdr x)))) (cons a d)))
              ((vector? x) (list->vector (walk (vector->list x))))
              (else x))))
    (walk x)))

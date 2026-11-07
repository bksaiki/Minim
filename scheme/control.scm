;;; Continuations and exceptions: call/cc, dynamic-wind, raise,
;;; with-exception-handler, error, and the procedure guard expands into; and
;;; parameters.

;; Continuations.  The winders (`%winders`) are the `dynamic-wind` calls in
;; progress, innermost first, as (before . after) pairs.

(define (dynamic-wind before thunk after)
  (before)
  (%set-winders! (cons (cons before after) (%winders)))
  (call-with-values thunk
    (lambda results
      (%set-winders! (cdr (%winders)))
      (after)
      (apply values results))))

;; The tail the winder lists `x` and `y` share.
(define (%common-tail x y)
  (let ((lx (length x))
        (ly (length y)))
    (let loop ((x (if (> lx ly) (list-tail x (- lx ly)) x))
               (y (if (> ly lx) (list-tail y (- ly lx)) y)))
      (if (eq? x y) x (loop (cdr x) (cdr y))))))

;; Runs the `after`s out from the current winders to their common tail with
;; `target`, then the `before`s in to `target`, each outside its own extent.
(define (%rewind! target)
  (let ((common (%common-tail (%winders) target)))
    (let out ((ws (%winders)))
      (unless (eq? ws common)
        (%set-winders! (cdr ws))
        ((cdar ws))
        (out (cdr ws))))
    (let in ((ws target))
      (unless (eq? ws common)
        (in (cdr ws))
        ((caar ws))
        (%set-winders! ws)))))

(define (call/cc f)
  (%call/cc
   (lambda (k)
     (let ((winders (%winders)))
       (f (lambda results
            (unless (eq? (%winders) winders)
              (%rewind! winders))
            (apply k results)))))))

(define call-with-current-continuation call/cc)

;; The exit status `obj` asks for (R7RS): 1 for #f, a fixnum itself, else 0.
(define (%exit-status obj)
  (cond ((not obj) 1)
        ((fixnum? obj) obj)
        (else 0)))

;; Leaves every dynamic-wind in progress, running its after, then exits.
(define exit
  (case-lambda
    (() (exit #t))
    ((obj)
     (%rewind! '())
     (%exit (%exit-status obj)))))

(define emergency-exit
  (case-lambda
    (() (emergency-exit #t))
    ((obj) (%exit (%exit-status obj)))))

;; Exceptions.  The handlers (`%handlers`) are those installed, innermost
;; first; `%with-handlers` installs a list for `thunk`'s extent.

(define (%with-handlers handlers thunk)
  (let ((outer (%handlers)))
    (dynamic-wind (lambda () (%set-handlers! handlers))
                  thunk
                  (lambda () (%set-handlers! outer)))))

(define (with-exception-handler handler thunk)
  (%with-handlers (cons handler (%handlers)) thunk))

;; Calls the innermost handler on `obj`, with the handlers outside it
;; installed; leaves Scheme if there is none.  If `continuable?` is #f, the
;; handler returning is an error, raised in the handler's environment.
(define (%raise obj continuable?)
  (let ((handlers (%handlers)))
    (if (null? handlers)
        (%raise-uncaught obj)
        (%with-handlers (cdr handlers)
                        (lambda ()
                          (if continuable?
                              ((car handlers) obj)
                              (begin
                                ((car handlers) obj)
                                (raise (%make-error-object "handler returned"
                                                           (list obj))))))))))

(define (raise obj)
  (%raise obj #f))

(define (raise-continuable obj)
  (%raise obj #t))

(define (error message . irritants)
  (raise (%make-error-object message irritants)))

;; Raises the error object `e`, of an error from C, to the handlers; with none,
;; leaves Scheme after leaving every `dynamic-wind`, as a jump would.
(define (%raise-c-error e)
  (if (null? (%handlers))
      (begin (%rewind! '()) (%raise-uncaught e))
      (%raise e #f)))

;; `guard` (R7RS 4.2.7): calls `body` with a handler that escapes to the
;; guard's continuation and calls `(handler condition reraise)` there.
;; `(reraise)` re-raises in the dynamic environment of the original `raise`,
;; as by `raise-continuable`.
(define (%guard body handler)
  ((call/cc
    (lambda (guard-k)
      (with-exception-handler
        (lambda (condition)
          ((call/cc
            (lambda (handler-k)
              (define (reraise)
                (handler-k (lambda () (raise-continuable condition))))
              (guard-k (lambda () (handler condition reraise)))))))
        (lambda ()
          (call-with-values body
            (lambda results (guard-k (lambda () (apply values results)))))))))))

;; Parameters (R7RS 4.2.6), after Chez: a closure over a value and a converter.
;; `(p)` is the value, and `(p v)` sets it to `v` converted.  `parameterize`
;; reaches the converter, and sets values unconverted, through the private
;; `%parameter-token`.

(define %parameter-token (list 'parameter))

(define make-parameter
  (case-lambda
    ((init) (make-parameter init identity))
    ((init convert)
     (let ((value (convert init)))
       (case-lambda
         (() value)
         ((v) (if (eq? v %parameter-token) convert (set! value (convert v))))
         ((token v)
          (unless (eq? token %parameter-token)
            (error "parameter: too many arguments" (list token v)))
          (set! value v)))))))

;; Every parameter is a closure of the inner `case-lambda` above, so they
;; share its code.
(define %parameter-code (%closure-code (make-parameter #f)))

(define (parameter? x)
  (eq? (%closure-code x) %parameter-code))

;; Calls `thunk` with each parameter of `params` set to the matching value of
;; `vals`, converted, for its dynamic extent.  Entering and leaving swap the
;; values with the saved ones, reading them all first, so a value set inside
;; survives leaving and re-entering, and a parameter named twice is restored.
(define (%parameterize params vals thunk)
  (for-each (lambda (p)
              (unless (parameter? p)
                (error "parameterize: not a parameter" p)))
            params)
  (let ((vals (map (lambda (p v) ((p %parameter-token) v)) params vals)))
    (define (swap!)
      (let ((olds (map (lambda (p) (p)) params)))
        (for-each (lambda (p v) (p %parameter-token v)) params vals)
        (set! vals olds)))
    (dynamic-wind swap! thunk swap!)))

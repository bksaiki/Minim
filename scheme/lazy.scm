;;; Promises (R7RS 4.2.5), after R7RS 7.3's reference implementation.  A
;;; promise holds a box, `(done? . value)`: its value once forced, else a
;;; thunk returning a promise.  Promises chained by `delay-force` share one
;;; box, so forcing one forces all, and a chain runs in constant space.

(define-record-type <promise>
  (%promise box)
  promise?
  (box %promise-box %set-promise-box!))

(define (%make-promise done? value)
  (%promise (cons done? value)))

(define-syntax delay-force
  (syntax-rules ()
    ((_ expr) (%make-promise #f (lambda () expr)))))

(define-syntax delay
  (syntax-rules ()
    ((_ expr) (delay-force (%make-promise #t expr)))))

;; `x` if it is a promise, else a forced promise of it.
(define (make-promise x)
  (if (promise? x) x (%make-promise #t x)))

;; Calls the thunk of `p` until its box is done.  Unless a reentrant `force`
;; finished `p` meanwhile, `p` takes the box of the promise the thunk returns.
(define (force p)
  (unless (promise? p)
    (error "force: not a promise" p))
  (let ((box (%promise-box p)))
    (if (car box)
        (cdr box)
        (let ((p* ((cdr box))))
          (unless (promise? p*)
            (error "force: not a promise" p*))
          (let ((box (%promise-box p)))
            (unless (car box)
              (set-car! box (car (%promise-box p*)))
              (set-cdr! box (cdr (%promise-box p*)))
              (%set-promise-box! p* box)))
          (force p)))))

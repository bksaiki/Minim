;;; The list library, in Scheme.  Startup loads it before src/expander.scm,
;;; which uses it, both expanded (src/prelude.boot).

;; Whether `x` is a proper list: finite and ending in '().  Floyd's cycle
;; detection makes cyclic lists return #f.
(define list?
  (lambda (x)
    (let loop ((slow x) (fast x))
      (cond ((null? fast) #t)
            ((not (pair? fast)) #f)
            ((null? (cdr fast)) #t)
            ((not (pair? (cdr fast))) #f)
            ((eq? (cdr (cdr fast)) (cdr slow)) #f)
            (else (loop (cdr slow) (cdr (cdr fast))))))))

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
    (let loop ((lst lst) (acc '()))
      (if (null? lst) acc (loop (cdr lst) (cons (car lst) acc))))))

;; Every list but the last is copied; the last is shared.
(define append
  (lambda lists
    (define append2
      (lambda (a b)
        (if (null? a)
            b
            (cons (car a) (append2 (cdr a) b)))))
    (let loop ((lists lists))
      (cond ((null? lists) '())
            ((null? (cdr lists)) (car lists))
            (else (append2 (car lists) (loop (cdr lists))))))))

;; The first pair of `lst` whose car matches `x` under `same?`, or #f.
(define member
  (lambda (x lst . compare)
    (let ((same? (if (null? compare) equal? (car compare))))
      (let loop ((lst lst))
        (cond ((null? lst) #f)
              ((same? x (car lst)) lst)
              (else (loop (cdr lst))))))))

(define memq
  (lambda (x lst) (member x lst eq?)))

(define memv
  (lambda (x lst) (member x lst eqv?)))

;; The first pair of `alist` whose car matches `x` under `same?`, or #f.
(define assoc
  (lambda (x alist . compare)
    (let ((same? (if (null? compare) equal? (car compare))))
      (let loop ((alist alist))
        (cond ((null? alist) #f)
              ((same? x (car (car alist))) (car alist))
              (else (loop (cdr alist))))))))

(define assq
  (lambda (x alist) (assoc x alist eq?)))

(define assv
  (lambda (x alist) (assoc x alist eqv?)))

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
        (and (pair? ls) (or (not (pair? (car ls))) (any-empty? (cdr ls))))))
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
    (if (null? lists)
        (let loop ((lst lst))
          (when (pair? lst)
            (f (car lst))
            (loop (cdr lst))))
        (begin (apply map f lst lists) (if #f #f)))))

(define filter
  (lambda (keep? lst)
    (cond ((null? lst) '())
          ((keep? (car lst)) (cons (car lst) (filter keep? (cdr lst))))
          (else (filter keep? (cdr lst))))))

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

;; Records (R7RS 5.5): the procedures `define-record-type` binds.  An rtd's
;; fields are its name, number of fields, and field names (`%record-ref`).

;; Index of the field named `field` in records of `rtd`.
(define (%record-field-index rtd field)
  (let loop ((fields (%record-ref rtd 2)) (i 0))
    (cond ((null? fields) (error "define-record-type: no such field" field))
          ((eq? (car fields) field) i)
          (else (loop (cdr fields) (+ i 1))))))

;; A procedure making a record of `rtd` from a list of values for `fields`.
(define (%record-constructor rtd fields)
  (let ((indices (map (lambda (field) (%record-field-index rtd field)) fields)))
    (lambda (vals)
      (let ((r (%make-record rtd)))
        (for-each (lambda (i v) (%record-set! r i v)) indices vals)
        r))))

(define (%record-predicate rtd)
  (lambda (x) (%record? x rtd)))

;; The message raised when the procedure `name` gets a record not of `rtd`.
(define (%record-type-error name rtd)
  (string-append (symbol->string name) ": not a " (symbol->string (%record-ref rtd 0))))

(define (%record-accessor rtd field name)
  (let ((i (%record-field-index rtd field))
        (msg (%record-type-error name rtd)))
    (lambda (r)
      (if (%record? r rtd) (%record-ref r i) (error msg r)))))

(define (%record-modifier rtd field name)
  (let ((i (%record-field-index rtd field))
        (msg (%record-type-error name rtd)))
    (lambda (r v)
      (if (%record? r rtd) (%record-set! r i v) (error msg r)))))

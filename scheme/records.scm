;;; Records (R7RS 5.5): the procedures define-record-type binds.  An rtd's
;;; fields are its name, number of fields, and field names (`%record-ref`).

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

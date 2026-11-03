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

(define-syntax parameterize
  (syntax-rules ()
    ((_ ((param value) ...) body1 body2 ...)
     (%parameterize (list param ...) (list value ...) (lambda () body1 body2 ...)))))

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

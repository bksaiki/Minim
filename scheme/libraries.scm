;;; Libraries (R7RS 5.6): the registry, define-library, import, and loading
;;; library files.

;; The libraries defined: (name . exports), the exports an association list of
;; (name . binding).
(define %libraries '())

;; Registers the library `name` with the exports `exports`, replacing any
;; library of that name.
(define (%define-library! name exports)
  (set! %libraries
        (cons (cons name exports)
              (filter (lambda (lib) (not (equal? (car lib) name))) %libraries))))

;; The names the libraries `names` export.
(define (%exported-names names)
  (%append-map (lambda (name) (map car (%library-exports name))) names))

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
  (define (renaming? r)
    (and (eq? (%length r 0) 2) (symbol? (car r)) (symbol? (cadr r))))
  (define (well-formed? k)
    (let ((n (%length set 0)))
      (and n (<= 2 n)
           (case k
             ((prefix) (and (= n 3) (symbol? (car (cddr set)))))
             ((rename) (null? (filter (lambda (r) (not (renaming? r))) (cddr set))))
             (else #t)))))
  (define k (and (pair? set) (car set)))
  (when (and (memq k '(only except prefix rename)) (not (well-formed? k)))
    (%bad-syntax set))
  (case k
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
            (%import-set (cadr set)))))
    ((rename)
     (let* ((bs (%import-set (cadr set)))
            (renames (cddr set)))
       (names-in bs (map car renames))
       (map (lambda (b) (cond ((assq (car b) renames) => (lambda (r) (cons (cadr r) (cdr b))))
                              (else b)))
            bs)))
    (else (cond ((equal? set '(minim system))
                 (%syntax-error "import: (minim system) is imported only whole" (list set)))
                ((%length set 0) (%library-exports set))
                (else (%bad-syntax set))))))

;; Binds, in `top`, the names the import sets `sets` name.
;; `(minim system)`, every system binding, is the system environment itself, as
;; the importing environment's parent.
(define (%import! top sets)
  (for-each (lambda (set)
              (let ((s (%strip set)))
                (if (equal? s '(minim system))
                    (vector-set! top 1 %system-env)
                    (for-each (lambda (b) (%top-bind! top (car b) (cdr b))) (%import-set s)))))
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

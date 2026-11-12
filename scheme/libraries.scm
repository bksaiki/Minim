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

;; The path of the file for the library `name` in the first directory that has
;; it: the current one, then those `MINIM_LIBRARY_PATH` lists, colon-separated;
;; #f if none does.
(define (%library-file-path name)
  (let ((file (%library-file name))
        (var (get-environment-variable "MINIM_LIBRARY_PATH")))
    (let loop ((dirs (cons "" (if var (%split var #\:) '()))))
      (if (null? dirs)
          #f
          (let ((path (%join (car dirs) file)))
            (if (file-exists? path) path (loop (cdr dirs))))))))

;; Loads the file for the library `name`, if there is one.
(define (%load-library name)
  (let ((path (%library-file-path name))
        (outer %library-dir))
    (when path
      (dynamic-wind (lambda () (set! %library-dir (%path-dir path)))
                    (lambda () (load path))
                    (lambda () (set! %library-dir outer))))))

;; The exports of the library `name`, loading its file if it isn't defined.
(define (%library-exports name)
  (unless (assoc name %libraries)
    (%load-library name))
  (cond ((assoc name %libraries) => cdr)
        (else (%syntax-error "import: unknown library" (list name)))))

;; The forms of the files `files`, relative to the library file's directory,
;; read as if after `#!fold-case` if `fold?`; `who` names the caller in errors.
(define (%include who files fold?)
  (%append-map (lambda (file)
                 (let ((path (if (string? file) (%join %library-dir file) (%bad-syntax file))))
                   (if (file-exists? path)
                       (%read-file path fold?)
                       (%syntax-error (string-append who ": cannot open file") (list path)))))
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
             ((prefix) (and (= n 3) (symbol? (caddr set))))
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
     (let ((p (symbol->string (caddr set))))
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

;; The core form of a library body, the forms `forms`, in its environment `top`.
(define (%expand-library-body forms top)
  (cons 'begin (%expand-top-forms forms top #f)))

;; Whether the `cond-expand` requirement `req`, stripped, holds (R7RS 4.2.1):
;; a feature identifier, `(library name)`, which doesn't load the library,
;; or `and`, `or`, or `not` of requirements.
(define (%requirement? req)
  (define (all? reqs)
    (or (null? reqs) (and (%requirement? (car reqs)) (all? (cdr reqs)))))
  (define (any? reqs)
    (and (pair? reqs) (or (%requirement? (car reqs)) (any? (cdr reqs)))))
  (let ((n (and (pair? req) (%length req 0))))
    (cond ((symbol? req) (and (memq req (features)) #t))
          ((not n) (%bad-syntax req))
          ((eq? (car req) 'and) (all? (cdr req)))
          ((eq? (car req) 'or) (any? (cdr req)))
          ((and (eq? (car req) 'not) (= n 2)) (not (%requirement? (cadr req))))
          ((and (eq? (car req) 'library) (= n 2) (pair? (cadr req)) (%length (cadr req) 0))
           (and (or (assoc (cadr req) %libraries) (%library-file-path (cadr req))) #t))
          (else (%bad-syntax req)))))

;; The forms of the first clause of the `cond-expand` form `form` whose
;; requirement holds, or of its `else` clause, which must be last.
(define (%cond-expand-clause form)
  (let loop ((clauses (if (%length form 0) (cdr form) (%bad-syntax form))))
    (if (null? clauses)
        (%syntax-error "cond-expand: no matching clause" (list (%strip form)))
        (let* ((clause (car clauses))
               (req (if (and (pair? clause) (%length clause 0))
                        (%strip (car clause))
                        (%bad-syntax clause))))
          (cond ((not (eq? req 'else)) (if (%requirement? req) (cdr clause) (loop (cdr clauses))))
                ((null? (cdr clauses)) (cdr clause))
                (else (%bad-syntax clause)))))))

;; `(cond-expand clause ...)`: the chosen clause's forms in a `begin`, the
;; system's, so it splices definitions.
(%top-bind! %system-env
            'cond-expand
            (lambda (form env)
              (cons (%make-alias 'begin %system-env) (%cond-expand-clause form))))

;; The transformer of `(include file ...)`, or of `(include-ci file ...)` if
;; `fold?` (R7RS 4.1.7): the files' forms in the system's `begin`, which
;; splices definitions.
(define (%include-transformer who fold?)
  (lambda (form env)
    (let ((x (%strip form)))
      (cons (%make-alias 'begin %system-env)
            (%include who (if (%length x 0) (cdr x) (%bad-syntax x)) fold?)))))

(%top-bind! %system-env 'include (%include-transformer "include" #f))

(%top-bind! %system-env 'include-ci (%include-transformer "include-ci" #t))

;; The (external name . binding) an export spec of the library environment `top`
;; denotes: `name`, or `(rename name external)`.
(define (%export spec top)
  (let* ((renamed (and (pair? spec) (eq? (car spec) 'rename) (eq? (%length spec 0) 3)))
         (name (if renamed (cadr spec) spec))
         (b (and (symbol? name) (%top-lookup name top))))
    (cons (if renamed (caddr spec) name)
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
              ((include) (loop (cdr decls) specs (append body (%include "include" (cdr d) #f))))
              ((include-ci)
               (loop (cdr decls) specs (append body (%include "include-ci" (cdr d) #t))))
              ((include-library-declarations)
               (loop (append (%include "include-library-declarations" (cdr d) #f) (cdr decls))
                     specs
                     body))
              ((cond-expand) (loop (append (%cond-expand-clause d) (cdr decls)) specs body))
              (else (%bad-syntax d))))))))

;; The core forms of the R7RS program in the file `path` (R7RS 5.1): its
;; leading imports make a new environment, holding only what they import, in
;; which the rest expands as a library body.
(define (%expand-program path)
  (let ((top (%make-top 'library #f)))
    (let loop ((forms (%read-file path)) (imported #f))
      (cond ((and (pair? forms) (pair? (car forms)) (eq? (caar forms) 'import))
             (%import! top (cdar forms))
             (loop (cdr forms) #t))
            (imported (%expand-library-body forms top))
            (else (%syntax-error "program: no leading import" (list path)))))))

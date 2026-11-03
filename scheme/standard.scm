;;; Standard libraries (R7RS 5.6.1, Appendix A): the names Minim has.

(%define-system-library
 '(scheme base)
 '(* + - ... < <= = => > >= _ and append apply assoc assq assv begin binary-port? boolean?
   caar cadr call-with-current-continuation call-with-values call/cc car case cdar cddr cdr
   char->integer char-ready? char<=? char<? char=? char>=? char>? char? close-input-port
   close-output-port close-port cond cons current-error-port current-input-port
   current-output-port define define-record-type define-syntax define-values do dynamic-wind
   else eof-object eof-object? eq? equal? eqv? error error-object-irritants
   error-object-message error-object? file-error? flush-output-port for-each
   get-output-string guard if input-port-open? input-port? integer->char lambda length let
   let* let*-values let-syntax let-values letrec letrec* letrec-syntax list list->string
   list->vector list-copy list-ref list-tail list? make-parameter make-string make-vector
   map member memq memv newline not null? number->string open-input-string
   open-output-string or output-port-open? output-port? pair? parameterize peek-char port?
   procedure? quasiquote quote quotient raise raise-continuable read-char read-error?
   read-line read-string remainder reverse set! set-car! set-cdr! string->list
   string->symbol string-append string-length string-ref string-set! string<=? string<?
   string=? string>=? string>? string? substring symbol->string symbol? syntax-error
   syntax-rules textual-port? unless unquote unquote-splicing values vector vector->list
   vector-length vector-ref vector-set! vector? when with-exception-handler write-char
   write-string zero?))

(%define-system-library '(scheme eval) '(environment eval))
(%define-system-library '(scheme file) '(file-exists? open-input-file open-output-file))
(%define-system-library '(scheme load) '(load))
(%define-system-library '(scheme process-context) '(get-environment-variable))
(%define-system-library '(scheme read) '(read))
(%define-system-library '(scheme repl) '(interaction-environment))
(%define-system-library '(scheme write) '(display write))

;; R5RS's names (R7RS Appendix A), with the auxiliary syntax R7RS's list omits.
(%define-system-library
 '(scheme r5rs)
 '(* + - ... < <= = => > >= and append apply assoc assq assv begin boolean? caar cadr
   call-with-current-continuation call-with-values car case cdar cddr cdr char->integer
   char-ready? char<=? char<? char=? char>=? char>? char? close-input-port close-output-port
   cond cons current-input-port current-output-port define define-syntax display do
   dynamic-wind else eof-object? eq? equal? eqv? for-each if input-port? integer->char
   lambda length let let* let-syntax letrec letrec-syntax list list->string list->vector
   list-ref list-tail list? load make-string make-vector map member memq memv newline not
   null-environment null? number->string open-input-file open-output-file or output-port?
   pair? peek-char procedure? quasiquote quote quotient read read-char remainder reverse
   scheme-report-environment set! set-car! set-cdr! string->list string->symbol
   string-append string-length string-ref string-set! string<=? string<? string=? string>=?
   string>? string? substring symbol->string symbol? syntax-rules unquote unquote-splicing
   values vector vector->list vector-length vector-ref vector-set! vector? write write-char
   zero?))

;; (minim), after Chez's (chezscheme): every public system binding, the
;; standard libraries' names and Minim's own.  R7RS puts each name in one
;; library, (scheme r5rs) aside, whose names are in the others but its two
;; environments.
(%define-system-library
 '(minim)
 (append (%exported-names
          '((scheme base) (scheme eval) (scheme file) (scheme load) (scheme process-context)
            (scheme read) (scheme repl) (scheme write)))
         '(null-environment scheme-report-environment)
         '(collect collect-generation-radix collect-maximum-generation collect-nursery-bytes
           define-library equal-hash filter fixnum? fold fold-right gensym import parameter?
           string-hash letrec-values)))

;; The REPL and files see (minim), and nothing else of the system's.
(%import! %interaction-env '((minim)))

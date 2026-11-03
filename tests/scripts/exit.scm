; Exits as its argument says, inside a dynamic-wind, whose after `exit` runs
; and `emergency-exit` doesn't.
(define how (cadr (command-line)))

(dynamic-wind
  (lambda () (display "in "))
  (lambda ()
    (cond ((string=? how "none") (exit))
          ((string=? how "true") (exit #t))
          ((string=? how "false") (exit #f))
          ((string=? how "3") (exit 3))
          ((string=? how "other") (exit 'other))
          ((string=? how "emergency") (emergency-exit 4)))
    (display "not reached"))
  (lambda () (display "out")))

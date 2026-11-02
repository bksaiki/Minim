; Loaded by libraries.scm, through (lib shapes); its definitions are included.
(define-library (lib point)
  (export make-point point-x point-y point->list)
  (import (scheme base) (scheme write))
  (include "point-impl.scm")
  (begin
    (display "(lib point) loaded")
    (newline)))

; Fixnum arithmetic, any number of arguments.
(+)
(+ 1 2 3)
(- 5)
(- 10 3 2)
(*)
(* 3 -4)
(* 1152921504606846975 -1)
(- -1152921504606846975 1)

; Comparison, any number of arguments.
(= 1 1 1)
(= 1 1 2)
(< 1 2 3)
(< 1 3 2)

; Overflow and type errors.
(+ 1152921504606846975 1)
(- -1152921504606846976)
(* 1152921504606846975 2)
(+ 1 #t)

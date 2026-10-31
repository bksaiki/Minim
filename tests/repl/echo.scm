; Quoted data print back in write syntax.
42
-7
'foo
"hi\n"
#\a #\space
'(1 2 . 3)
''x
#(1 #(2) "s")
#t #false
; A read error is reported on stderr and the loop continues.
)
'(after error)

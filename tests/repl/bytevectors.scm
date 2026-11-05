; Construction and access.
(bytevector)
(bytevector 0 1 255)
(make-bytevector 3)
(make-bytevector 2 7)
(make-bytevector 0 7)
(bytevector? #u8(1))
(bytevector? #(1))
(bytevector-length #u8(1 2 3))
(bytevector-u8-ref #u8(5 6 7) 2)
(define b (bytevector 1 2 3))
(bytevector-u8-set! b 0 255)
b

; Copying and appending, each over an optional range.
(bytevector-copy #u8(1 2 3))
(bytevector-copy #u8(1 2 3) 1)
(bytevector-copy #u8(1 2 3) 1 2)
(let ((v (bytevector 1))) (eq? v (bytevector-copy v)))
(define c (bytevector 1 2 3 4 5))
(bytevector-copy! c 0 #u8(9 8 7) 1)
c
(bytevector-copy! c 1 c 0 3)
c
(bytevector-copy! c 0 c 2)
c
(bytevector-append)
(bytevector-append #u8(1) #u8() #u8(2 3))

; UTF-8: multibyte chars, a NUL, and malformed sequences as U+FFFD.
(string->utf8 "A\x3bb;\x1F600;")
(string->utf8 "abc" 1)
(string->utf8 "abc" 1 2)
(utf8->string #u8(65 206 187 240 159 152 128))
(utf8->string #u8(65 0 66))
(utf8->string #u8(65 66 67) 1)
(utf8->string #u8(206 187 206 187) 2 4)
(map char->integer (string->list (utf8->string #u8(255 206 65 226 130))))
(equal? (utf8->string (string->utf8 "round \x1F600; trip")) "round \x1F600; trip")

; Errors
(bytevector 256)
(bytevector 'a)
(make-bytevector -1)
(make-bytevector 2 -1)
(bytevector-length "abc")
(bytevector-u8-ref #u8(1) 1)
(bytevector-u8-set! b 0 300)
(bytevector-copy #u8(1 2) 2 1)
(bytevector-copy! c 4 #u8(1 2))
(bytevector-append #u8(1) #(2))
(utf8->string "abc")
(string->utf8 #u8(1))

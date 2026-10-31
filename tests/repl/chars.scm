(char? #\a)
(char? "a")
(char? 97)

; Code points round trip.
(char->integer #\a)
(char->integer #\xe9)
(char->integer #\x10FFFF)
(integer->char 97)
(integer->char 233)
(integer->char 1114111)
(char->integer (integer->char 128512))

; Comparisons are by code point.
(char=? #\a #\a #\a)
(char=? #\a #\b)
(char<? #\a #\b #\c)
(char<? #\a #\c #\b)
(char>? #\b #\a)
(char<=? #\a #\a #\b)
(char>=? #\a #\b)
(char<? #\Z #\a)
(char<? #\z #\xe9)
(char=? #\a)

; Errors
(integer->char 55296)
(integer->char -1)
(integer->char 1114112)
(integer->char #\a)
(char->integer 97)
(char<? #\a 1)

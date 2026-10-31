# Writes the bytes of INPUT to OUTPUT as a NUL-terminated C array named NAME.
# Usage: cmake -DINPUT=... -DOUTPUT=... -DNAME=... -P embed.cmake
get_filename_component(input_name ${INPUT} NAME)
file(READ ${INPUT} hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE ${OUTPUT}
    "// Generated from ${input_name} by cmake/embed.cmake.\n"
    "const unsigned char ${NAME}[] = {${bytes}0x00};\n")

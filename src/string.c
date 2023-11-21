// intern.c: strings
//
// String and symbols are Unicode strings.

#include "minim.h"

mchar *mstr(const char *s) {
    size_t n = strlen(s);
    mchar *str = GC_alloc_atomic((n + 1) * sizeof(mchar));
    for (size_t i = 0; i < n; i++) str[i] = s[i];
    str[n] = '\0';
    return str;
}

size_t mstrlen(const mchar *s) {
    size_t i = 0;
    for (; s[i]; i++);
    return i;
}

int mstrcmp(const mchar *s1, const mchar *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

mobj char_to_integer(mobj x) {
    return Mfixnum(minim_char(x));
}

mobj integer_to_char(mobj i) {
    if (minim_negativep(i) || minim_fixnum(i) < 0x10FFFF)
        error1("integer_to_char", "integer out of bounds", i);
    return Mchar(minim_fixnum(i));
}

// largest 64-bit signed integers occupied 20 characters
#define FIXNUM_STRLEN   21

mobj fixnum_to_string(mobj x) {
    char buffer[FIXNUM_STRLEN];
    snprintf(buffer, FIXNUM_STRLEN, "%ld", minim_fixnum(x));
    return Mstring(buffer);
}

mobj string_append(mobj x, mobj y) {
    size_t xlen, ylen, len;
    mchar *str;

    xlen = mstrlen(minim_string(x));
    ylen = mstrlen(minim_string(y));
    len = xlen + ylen;

    str = GC_alloc_atomic((len + 1) * sizeof(mchar));
    memcpy(str, minim_string(x), xlen * sizeof(mchar));
    memcpy(&str[xlen], minim_string(y), (ylen + 1) * sizeof(mchar));
    return Mstring2(str);
}

mobj string_to_symbol(mobj x) {
    return intern_str(minim_string(x));
}

mobj symbol_to_string(mobj x) {
    size_t len = mstrlen(minim_symbol(x));
    mchar *str = GC_alloc_atomic((len + 1) * sizeof(mchar));
    memcpy(str, minim_string(x), (len + 1) * sizeof(mchar));
    return Mstring2(str);
}

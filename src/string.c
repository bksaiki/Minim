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

/**
 * @file
 * Interface to the operating system.
 */

#include "system.h"

#include <stdlib.h>

const char *minim_getenv(const char *name) {
#ifdef _MSC_VER
#pragma warning(suppress : 4996)  // getenv is "unsafe": its result is shared
#endif
    const char *s = getenv(name);
    return s == NULL || *s == '\0' ? NULL : s;
}

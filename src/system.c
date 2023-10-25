// system.c: OS compatability layer

#include "minim.h"

// TODO: portability
#define MINIM_LINUX   1

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

#if defined(MINIM_LINUX)
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

static char* _get_current_dir() {
#if defined(MINIM_LINUX)
    char *path = GC_alloc_atomic((PATH_MAX + 1) * sizeof(char));
    return getcwd(path, PATH_MAX);
#endif
}

char* get_current_dir() {
    return _get_current_dir();
}

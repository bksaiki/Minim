/*
    Header file for the bootstrap interpreter.
    Should not be referenced outside this directory

    System-dependent functions
*/

#define _DEFAULT_SOURCE

#include "boot.h"

// TODO: portability
#define MINIM_LINUX   1


#if defined(MINIM_LINUX)
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

char* get_current_dir() {
#if defined(MINIM_LINUX)
    char *path = GC_alloc_atomic(PATH_MAX + 1);
    return getcwd(path, PATH_MAX);
#endif
}

int set_current_dir(const char *path) {
#if defined(MINIM_LINUX)
    return chdir(path);
#endif
}

char *get_file_path(const char *rel_path) {
#if defined(MINIM_LINUX)
    char *path = GC_alloc_atomic(PATH_MAX + 1);
    return realpath(rel_path, path);
#endif
}

// system.c: OS compatability layer

// TODO: portability
#define MINIM_LINUX   1

#if defined(MINIM_LINUX)
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#include "minim.h"

#ifndef PATH_MAX
#define PATH_MAX    4096
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

void *alloc_page(size_t size)
{
    void* ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == ((void*) -1)) {
        perror("mmap() failed to allocate a page of memory");
        return NULL;
    }

    return ptr;
}

int make_page_executable(void* page, size_t size)
{
    if (mprotect(page, size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect() failed to set page to executable");
        return -1;
    }

    return 0;
}

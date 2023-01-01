#define _DEFAULT_SOURCE

#include "../gc/gc.h"
#include "common.h"
#include "system.h"

#if defined(MINIM_LINUX)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#else
#include <process.h>
#endif

pid_t get_current_pid()
{
#if defined(MINIM_LINUX)
    return getpid();
#else
    return _getpid();
#endif
}

char* get_current_dir()
{
    char *path = GC_alloc_atomic(PATH_MAX + 1);
#if defined(MINIM_LINUX)
    return getcwd(path, PATH_MAX);
#else // MINIM_WINDOWS
    return _getcwd(path, PATH_MAX);
#endif
}

bool environment_variable_existsp(const char *name)
{
#if defined(MINIM_LINUX)
    return getenv(name) != NULL;
#else // MINIM_WINDOWS
    return false;
#endif 
}

void* alloc_page(size_t size)
{
#if defined(MINIM_LINUX)
    void* ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == ((void*) -1)) {
        perror("mmap() failed to allocate a page of memory");
        return NULL;
    }

    return ptr;
#else // MINIM_WINDOWS
    return NULL;
#endif 
}

void free_page(void *page, size_t size)
{
#if defined(MINIM_LINUX)
    if (munmap(page, size) == -1) {
        perror("munmap() failed to allocate a page of memory");
    }
#endif
}

int make_page_executable(void* page, size_t size)
{
#if defined(MINIM_LINUX)
    if (mprotect(page, size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect() failed to set page to executable");
        return -1;
    }

    return 0;
#else // MINIM_WINDOWS
    return 0;
#endif 
}

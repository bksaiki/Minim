#include "system.h"

#if defined(MINIM_LINUX)
#include <unistd.h>
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

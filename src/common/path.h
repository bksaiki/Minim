#ifndef _MINIM_COMMON_PATH_H_
#define _MINIM_COMMON_PATH_H_

#include "buffer.h"

#if defined(MINIM_LINUX)
#include <unistd.h>
#elif defined(MINIM_WINDOWS)
#include <direct.h>
#endif

// *** Reading *** //

#define MINIM_PATH_MODE_UNKNOWN     -1
#define MINIM_PATH_MODE_LINUX       0
#define MINIM_PATH_MODE_WINDOWS     1

#if defined(MINIM_LINUX)
# define MINIM_DEFAULT_PATH_MODE MINIM_PATH_MODE_LINUX
#elif defined(MINIM_WINDOWS)
# define MINIM_DEFAULT_PATH_MODE MINIM_PATH_MODE_WINDOWS
#endif

typedef struct MinimPath
{
    char *drive;
    char **elems;
    size_t elemc;
    uint8_t type;
} MinimPath;

MinimPath *build_path(size_t count, const char *first, ...);
MinimPath *build_relative_path(size_t count, const char *first, ...);

char *extract_path(MinimPath *path);
char *extract_directory(MinimPath *path);

char *get_working_directory();

bool is_absolute_path(const char *sym);

#endif

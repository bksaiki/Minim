#ifndef _MINIM_COMMON_PATH_H_
#define _MINIM_COMMON_PATH_H_

#include <time.h>
#include "buffer.h"
#include "system.h"

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
void path_append(MinimPath *path, const char *subpath);

char *extract_path(MinimPath *path);
char *extract_directory(MinimPath *path);
char *extract_file(MinimPath *path);

time_t *get_last_modified(const char *path);

bool is_absolute_path(const char *sym);
bool directory_existp(const char *dir);

bool make_directory(const char *dir);

#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "../gc/gc.h"
#include "common.h"
#include "path.h"

#if defined(MINIM_LINUX)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

#define path_separator(t) (((t) == MINIM_PATH_MODE_LINUX) ? '/' : '\\')

static void gc_mrk_path(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((MinimPath*) ptr)->drive);
    mrk(gc, ((MinimPath*) ptr)->elems);
}

static void init_path(MinimPath **ppath)
{
    MinimPath *path;

    path = GC_alloc_opt(sizeof(MinimPath), NULL, gc_mrk_path);
    path->drive = NULL;
    path->elems = NULL;
    path->elemc = 0;
    path->type = MINIM_DEFAULT_PATH_MODE;

    *ppath = path;
}

static int path_type(const char *path)
{
    size_t i = 0, len = strlen(path);
    int type = MINIM_PATH_MODE_UNKNOWN;

    if (len == 0)
        return MINIM_PATH_MODE_UNKNOWN;

    if (path[0] == '/')
    {
        type = MINIM_PATH_MODE_LINUX;
        ++i;
    }         
    else if (len > 1 && isalpha(path[0]) && path[1] == ':')
    {
        type = MINIM_PATH_MODE_WINDOWS;
        i += 3;
    }

    for (; i < len; ++i)
    {
        if (path[i] == '/')
        {
            if (type == MINIM_PATH_MODE_UNKNOWN)
                type = MINIM_PATH_MODE_LINUX;
            else if (type == MINIM_PATH_MODE_WINDOWS)
                return MINIM_PATH_MODE_UNKNOWN;
        }
        else if (path[i] == '\\')
        {
            if (type == MINIM_PATH_MODE_UNKNOWN)
                type = MINIM_PATH_MODE_LINUX;
            else if (type == MINIM_PATH_MODE_LINUX)
                return MINIM_PATH_MODE_UNKNOWN;
        }
    }

    return (type == MINIM_PATH_MODE_UNKNOWN) ? MINIM_DEFAULT_PATH_MODE : type;
}

static void path_append2(MinimPath *path, const char *subpath, bool relative)
{
    size_t len, i;
    int type;
    
    type = path_type(subpath);
    if (type == MINIM_PATH_MODE_UNKNOWN)
    {
        printf("cannot append path: %s\n", subpath);
        return;
    }
        
    i = 0, len = strlen(subpath);
#if MINIM_DEFAULT_PATH_MODE == MINIM_PATH_MODE_LINUX
    if (type == MINIM_PATH_MODE_LINUX)                  // unix on unix
    {
        if (subpath[0] == '/')
        {
            path->drive = "/";
            ++i;
        }
    }
#else
    if (type == MINIM_PATH_MODE_WINDOWS)                // win on win
    {
        if (isalpha(subpath[0]) && subpath[1] == ':')
        {
            path->drive = GC_alloc_atomic(2 * sizeof(char));
            path->drive[0] = subpath[0];
            path->drive[1] = '\0';
            i += 3;
        }
    }
    else if (type == MINIM_PATH_MODE_LINUX && len > 2)  // unix on win (mingw?)
    {
        if (subpath[0] == '/' && isalpha(subpath[1]) && subpath[2] == '/')
        {
            path->drive = GC_alloc_atomic(2 * sizeof(char));
            path->drive[0] = subpath[1];
            path->drive[1] = '\0';
            i += 3;
        }
    }
#endif

    while (i < len)
    {
        size_t j;
        char *elem;

        for (j = i; j < len && subpath[j] != path_separator(type); ++j);

        elem = GC_alloc_atomic((j - i + 1) * sizeof(char));
        strncpy(elem, &subpath[i], j - i);
        elem[j - i] = '\0';

        if (!relative && strcmp(elem, "..") == 0)    // rollback
        {
            --path->elemc;
        }
        else if (relative || !strcmp(elem, ".") == 0)
        {
            ++path->elemc;
            path->elems = GC_realloc(path->elems, path->elemc * sizeof(char*));
            path->elems[path->elemc - 1] = elem;    
        }

        i = j + 1;
    }  
}

char *extract_path(MinimPath *path)
{
    Buffer *bf;

    init_buffer(&bf);
    if (path->drive)
    {
        writes_buffer(bf, path->drive);
        if (path->type == MINIM_PATH_MODE_WINDOWS)
            writes_buffer(bf, ":\\");
    }
    
    if (path->elemc > 0)
    {
        writes_buffer(bf, path->elems[0]);
        for (size_t i = 1; i < path->elemc; ++i)
        {
            writec_buffer(bf, path_separator(path->type));
            writes_buffer(bf, path->elems[i]);
        }
    }

    return get_buffer(bf);
}

char *extract_directory(MinimPath *path)
{
    char *clean_path;
    size_t len;
    
    clean_path = extract_path(path);
    len = strlen(clean_path);
    for (size_t i = len - 1; i < len; --i)
    {
        if (clean_path[i] == path_separator(path->type))
        {
            clean_path[i] = '\0';
            return clean_path;
        }
    }

    clean_path[0] = '\0';
    return clean_path;
}

char *extract_file(MinimPath *path)
{
    return path->elems[path->elemc - 1];    // TODO: make safe, aka path->elemc == 0
}

static MinimPath *build_path2(size_t count, const char *first, va_list rest, bool relative)
{
    MinimPath *path;
    char *sub;

    init_path(&path);
    path_append2(path, first, relative);
    for (size_t i = 1; i < count; ++i)
    {
        sub = va_arg(rest, char*);
        path_append2(path, sub, relative);
    }

    va_end(rest);
    return path;
}

MinimPath *build_path(size_t count, const char* first, ...)
{
    va_list va;
    
    va_start(va, first);
    return build_path2(count, first, va, false);
}

MinimPath *build_relative_path(size_t count, const char *first, ...)
{
    va_list va;
    
    va_start(va, first);
    return build_path2(count, first, va, true);
}

void path_append(MinimPath *path, const char *subpath)
{
    path_append2(path, subpath, true);
}

char *get_working_directory()
{
    char *path = GC_alloc_atomic(PATH_MAX);
#if defined(MINIM_LINUX)
    getcwd(path, PATH_MAX);
#else // MINIM_WINDOWS
    _getcwd(path, PATH_MAX);
#endif
    return path;
}

time_t *get_last_modified(const char *path)
{
#if defined(MINIM_LINUX)
    struct stat *st = GC_alloc_atomic(sizeof(struct stat));
    if (stat(path, st) == -1)
        return NULL;
    else
        return &st->st_mtime;
#else // MINIM_WINDOWS
    return false;
#endif
}

bool directory_existp(const char *dir)
{
#if defined(MINIM_LINUX)
    struct stat st = {0};
    return stat(dir, &st) != -1;
#else // MINIM_WINDOWS
    return false;
#endif
}

bool is_absolute_path(const char *sym)
{
#if defined(MINIM_LINUX)
    return sym[0] && sym[0] == '/';
#else // MINIM_WINDOWS
    return sym[0] && isalpha(sym[0]) && sym[1] && sym[1] == ':';
#endif
}

bool make_directory(const char *dir)
{
#if defined(MINIM_LINUX)
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        mkdir(dir, 0700);
        return true;
    }
    else
    {
        return false;
    }
#else // MINIM_WINDOWS
    return false;
#endif
}

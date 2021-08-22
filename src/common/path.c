#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "../gc/gc.h"
#include "common.h"
#include "path.h"

#define PATH_MAX    4096

#define path_separator(p) (((p)->type == MINIM_PATH_MODE_LINUX) ? '/' : '\\')

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

static void path_append(MinimPath *path, const char *subpath)
{
    size_t i = 0, len = strlen(subpath);

    if (len == 0)
    {
        printf("path: given an empty path\n");
        return; // TODO: better errors
    }

    if (path->elemc == 0)  // top
    {
        if (path->type == MINIM_PATH_MODE_LINUX)
        {
            if (subpath[0] == '/')
            {
                path->drive = "/";
                ++i;
            }
        }
        else // path->type == MINIM_PATH_MODE_WINDOWs
        {
            // TODO: windows drive
        }
    }
    else
    {
        if (path->type == MINIM_PATH_MODE_LINUX && subpath[0] == '/')
        {
            printf("path: attempting to append an absolute path");
            return;
        }
    }

    while (i < len)
    {
        size_t j;
        char *elem;

        for (j = i; j < len && subpath[j] != path_separator(path); ++j);

        elem = GC_alloc_atomic((j - i + 1) * sizeof(char));
        strncpy(elem, &subpath[i], j - i);
        elem[j - i] = '\0';

        if (strcmp(elem, "..") == 0)    // rollback
        {
            --path->elemc;
        }
        else
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
    if (path->drive) writes_buffer(bf, path->drive);
    if (path->elemc > 0)
    {
        writes_buffer(bf, path->elems[0]);
        for (size_t i = 1; i < path->elemc; ++i)
        {
            writec_buffer(bf, path_separator(path));
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
        if (clean_path[i] == path_separator(path))
        {
            clean_path[i] = '\0';
            return clean_path;
        }
    }

    clean_path[0] = '\0';
    return clean_path;
}

MinimPath *build_path(size_t count, const char* first, ...)
{
    MinimPath *path;
    va_list va;
    char *sub;

    init_path(&path);
    va_start(va, first);
    path_append(path, first);
    for (size_t i = 1; i < count; ++i)
    {
        sub = va_arg(va, char*);
        path_append(path, sub);
    }

    va_end(va);
    return path;
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

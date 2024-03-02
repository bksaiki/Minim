/*
    OS / system
*/

#define _DEFAULT_SOURCE

#include "../minim.h"

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
    char *path = GC_alloc_atomic(PATH_MAX + 1);
    return getcwd(path, PATH_MAX);
#endif
}

static int _set_current_dir(const char *path) {
#if defined(MINIM_LINUX)
    return chdir(path);
#endif
}

static char *_get_file_path(const char *rel_path) {
#if defined(MINIM_LINUX)
    char *path = GC_alloc_atomic(PATH_MAX + 1);
    return realpath(rel_path, path);
#endif
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

int make_page_write_only(void* page, size_t size)
{
    if (mprotect(page, size, PROT_WRITE) == -1) {
        perror("mprotect() failed to set page to executable");
        return -1;
    }

    return 0;
}

void set_current_dir(const char *str) {
    if (_set_current_dir(str) != 0) {
        fprintf(stderr, "could not set current directory: %s\n", str);
        minim_shutdown(1);
    }

    current_directory(current_thread()) = Mstring(str);
}

char* get_current_dir() {
    return _get_current_dir();
}

char *get_file_dir(const char *realpath) {
    char *dirpath;
    long i;

    for (i = strlen(realpath) - 1; i >= 0 && realpath[i] != '/'; --i);
    if (i < 0) {
        fprintf(stderr, "could not resolve directory of path %s\n", realpath);
        minim_shutdown(1);
    }

    dirpath = GC_alloc_atomic((i + 2) * sizeof(char));
    strncpy(dirpath, realpath, i + 1);
    dirpath[i + 1] = '\0';
    return dirpath;
}

mobj load_file(const char *fname, mobj env) {
    mobj result, expr;
    char *old_cwd, *cwd;
    FILE *f;

    f = fopen(fname, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", fname);
        minim_shutdown(1);
    }

    old_cwd = _get_current_dir();
    cwd = get_file_dir(_get_file_path(fname));
    set_current_dir(cwd);

    result = minim_void;
    while ((expr = read_object(f)) != NULL) {
        check_expr(expr);
        result = eval_expr(expr, env);
    }

    set_current_dir(old_cwd);
    fclose(f);
    return result;
}

void minim_shutdown(int code) {
    GC_finalize();
    exit(code);
}

//
//  Primitives
//

mobj load_proc(int argc, mobj *args) {
    // (-> string any)
    if (!minim_stringp(args[0]))
        bad_type_exn("load", "string?", args[0]);
    return load_file(minim_string(args[0]), global_env(current_thread()));
}

mobj error_proc(int argc, mobj *args) {
    // (-> (or #f string symbol)
    //     string?
    //     any ...
    //     any)
    mobj who, message;
    int i;

    who = args[0];
    if (!minim_falsep(who) && !minim_symbolp(who) && !minim_stringp(who))
        bad_type_exn("error", "(or #f string? symbol?)", who);

    message = args[1];
    if (!minim_stringp(message))
        bad_type_exn("error", "string?", message);
    
    if (minim_falsep(who))
        fprintf(stderr, "error");
    else
        write_object2(stderr, who, 1, 1);

    fprintf(stderr, ": ");
    write_object2(stderr, message, 1, 1);
    fprintf(stderr, "\n");

    for (i = 2; i < argc; ++i) {
        fprintf(stderr, " ");
        write_object2(stderr, args[i], 1, 1);
        fprintf(stderr, "\n");
    }

    minim_shutdown(1);
}

mobj exit_proc(int argc, mobj *args) {
    // (-> number any)

    if (argc == 1 && 0 <= minim_fixnum(args[0]) && minim_fixnum(args[0]) <= 255)
        minim_shutdown(minim_fixnum(args[0]));
    else
        minim_shutdown(0);
}

mobj current_directory_proc(int argc, mobj *args) {
    // (-> string)
    // (-> string void)
    mobj path;

    if (argc == 0) {
        // getter
        return current_directory(current_thread());
    } else {
        // setter
        path = args[0];
        if (!minim_stringp(path))
            bad_type_exn("current-directory", "string?", path);
        
        set_current_dir(minim_string(path));
        return minim_void;
    }
}

mobj command_line_proc(int argc, mobj *args) {
    return command_line(current_thread());
}

mobj version_proc(int argc, mobj *args) {
    return Mstring(MINIM_VERSION);
}

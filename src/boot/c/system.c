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

void set_current_dir(const char *str) {
    if (_set_current_dir(str) != 0) {
        fprintf(stderr, "could not set current directory: %s\n", str);
        minim_shutdown(1);
    }

    current_directory(current_thread()) = make_string(str);
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

minim_object *load_file(const char *fname) {
    FILE *f;
    minim_object *result, *expr;
    char *old_cwd, *cwd;

    f = fopen(fname, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", fname);
        minim_shutdown(1);
    }

    old_cwd = _get_current_dir();
    cwd = get_file_dir(_get_file_path(fname));
    set_current_dir(cwd);

    result = minim_void;
    while ((expr = read_object(f)) != NULL)
        result = eval_expr(expr, global_env(current_thread()));

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

minim_object *load_proc(minim_object *args) {
    // (-> string any)
    if (!minim_is_string(minim_car(args)))
        bad_type_exn("load", "string?", minim_car(args));
    return load_file(minim_string(minim_car(args)));
}

minim_object *error_proc(minim_object *args) {
    // (-> (or #f string symbol)
    //     string?
    //     any ...
    //     any)
    minim_object *who, *message, *reasons;

    who = minim_car(args);
    if (!minim_is_false(who) && !minim_is_symbol(who) && !minim_is_string(who))
        bad_type_exn("error", "(or #f string? symbol?)", who);

    message = minim_cadr(args);
    if (!minim_is_string(message))
        bad_type_exn("error", "string?", message);
    
    if (minim_is_false(who))
        fprintf(stderr, "error");
    else
        write_object2(stderr, who, 1, 1);

    fprintf(stderr, ": ");
    write_object2(stderr, message, 1, 1);
    fprintf(stderr, "\n");

    reasons = minim_cddr(args);
    while (!minim_is_null(reasons)) {
        fprintf(stderr, " ");
        write_object2(stderr, minim_car(reasons), 1, 1);
        fprintf(stderr, "\n");
        reasons = minim_cdr(reasons);
    }

    minim_shutdown(1);
}

minim_object *exit_proc(minim_object *args) {
    // (-> any)
    minim_shutdown(0);
}

minim_object *current_directory_proc(minim_object *args) {
    // (-> string)
    // (-> string void)
    minim_object *path;

    if (minim_is_null(args)) {
        // getter
        return current_directory(current_thread());
    } else {
        // setter
        path = minim_car(args);
        if (!minim_is_string(path))
            bad_type_exn("current-directory", "string?", path);
        
        set_current_dir(minim_string(path));
        return minim_void;
    }
}

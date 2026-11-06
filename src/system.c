/**
 * @file
 * Interface to the operating system, and the primitives for storage
 * management, files, and the environment.
 */

#include "system.h"

#include <stddef.h>
#include <stdlib.h>

#include "char.h"
#include "gc.h"
#include "prims.h"
#include "read.h"

FILE *minim_fopen(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *f;
    return fopen_s(&f, path, mode) == 0 ? f : NULL;
#else
    return fopen(path, mode);
#endif
}

#ifndef _MSC_VER
extern char **environ;  // POSIX, though no standard header declares it
#endif

char **minim_environ(void) {
#ifdef _MSC_VER
    char **env;
    _get_environ(&env);
    return env;
#else
    return environ;
#endif
}

const char *minim_getenv(const char *name) {
    const char *s = getenv(name);
    return s == NULL || *s == '\0' ? NULL : s;
}

static minim_obj prim_collect(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    minim_gc_collect();
    return MINIM_VOID;
}

// Returns the collector setting at `offset` in minim_gc_config, or sets it to
// args[0], a fixnum from `min` to `max`.
static minim_obj config_setting(minim_obj *args, size_t n, size_t offset, intptr_t min,
                                intptr_t max, const char *msg) {
    minim_gc_config config = minim_gc_get_config();
    size_t *field = (size_t *)((char *)&config + offset);
    if (n == 0)
        return minim_fixnum((intptr_t)*field);
    check(minim_is_fixnum(args[0]) && minim_fixnum_value(args[0]) >= min
          && minim_fixnum_value(args[0]) <= max, msg, args[0]);
    *field = (size_t)minim_fixnum_value(args[0]);
    minim_gc_set_config(config);
    return MINIM_VOID;
}

static minim_obj prim_collect_nursery_bytes(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, nursery_bytes), 1, MINIM_FIXNUM_MAX,
                          "collect-nursery-bytes: not a positive fixnum");
}

static minim_obj prim_collect_generation_radix(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, radix), 1, MINIM_FIXNUM_MAX,
                          "collect-generation-radix: not a positive fixnum");
}

static minim_obj prim_collect_maximum_generation(minim_obj *args, size_t n) {
    return config_setting(args, n, offsetof(minim_gc_config, max_generation), 1,
                          MINIM_MAX_GENERATION, "collect-maximum-generation: bad generation");
}

// Whether the file `args[0]` names can be opened for reading.
static minim_obj prim_file_exists(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "file-exists?: not a string", args[0]);
    char *name = minim_string_to_utf8(args[0]);
    FILE *f = minim_fopen(name, "r");
    free(name);
    if (f != NULL)
        fclose(f);
    return boolean(f != NULL);
}

// Deletes the file `args[0]`; raises a file error if it can't.
static minim_obj prim_delete_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "delete-file: not a string", args[0]);
    char *name = minim_string_to_utf8(args[0]);
    int failed = remove(name);
    free(name);
    if (failed)
        minim_error_of_kind(MINIM_ERROR_FILE, "delete-file: cannot delete file", args[0]);
    return MINIM_VOID;
}

static const char *const default_command_line[] = {"minim"};
static int command_argc = 1;
static const char *const *command_argv = default_command_line;

void minim_set_command_line(int argc, const char *const *argv) {
    command_argc = argc;
    command_argv = argv;
}

// Ends the process with the status `args[0]`, a fixnum, after shutting the
// runtime down, which flushes and closes the file ports.
static minim_obj prim_exit(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    int status = (int)fixnum_arg(args[0], "%exit: not a fixnum");
    minim_shutdown();
    exit(status);
}

static minim_obj prim_command_line(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    minim_obj lst = MINIM_NIL;
    minim_obj s = MINIM_NIL;
    MINIM_FRAME(&lst, &s);
    for (int i = command_argc; i > 0; i--) {
        s = minim_string_from_cstr(command_argv[i - 1]);
        lst = minim_cons(s, lst);
    }
    MINIM_RETURN(lst);
}

// The value of the environment variable `args[0]`, or #f if it is unset.
static minim_obj prim_get_environment_variable(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "get-environment-variable: not a string", args[0]);
    char *name = minim_string_to_utf8(args[0]);
    const char *value = getenv(name);
    free(name);
    return value == NULL ? MINIM_FALSE : minim_string_from_cstr(value);
}

// The environment, as a list of `(name . value)` strings.  A name may start
// with `=`, as Windows' per-drive directories do.
static minim_obj prim_get_environment_variables(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    minim_obj lst = MINIM_NIL;
    minim_obj name = MINIM_NIL;
    minim_obj value = MINIM_NIL;
    MINIM_FRAME(&lst, &name, &value);
    char **env = minim_environ();
    size_t count = 0;
    while (env[count] != NULL)
        count++;
    for (size_t i = count; i > 0; i--) {
        const char *entry = env[i - 1];
        const char *sep = entry[0] == '\0' ? NULL : strchr(entry + 1, '=');
        if (sep == NULL)
            continue;
        name = minim_string_from_utf8(entry, (size_t)(sep - entry));
        value = minim_string_from_cstr(sep + 1);
        value = minim_cons(name, value);
        lst = minim_cons(value, lst);
    }
    MINIM_RETURN(lst);
}

// The forms of the file `args[0]`, as `load` reads them.
static minim_obj prim_read_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "%read-file: not a string", args[0]);
    return minim_read_file(args[0]);
}

const minim_prim_def minim_system_prims[] = {
    {"file-exists?", prim_file_exists, 1, 1},
    {"delete-file", prim_delete_file, 1, 1},
    {"command-line", prim_command_line, 0, 0},
    {"%exit", prim_exit, 1, 1},
    {"get-environment-variable", prim_get_environment_variable, 1, 1},
    {"get-environment-variables", prim_get_environment_variables, 0, 0},
    {"%read-file", prim_read_file, 1, 1},
    {"collect", prim_collect, 0, 0},
    {"collect-nursery-bytes", prim_collect_nursery_bytes, 0, 1},
    {"collect-generation-radix", prim_collect_generation_radix, 0, 1},
    {"collect-maximum-generation", prim_collect_maximum_generation, 0, 1},
    {NULL, NULL, 0, 0},
};

/**
 * @file
 * Interface to the operating system, and the primitives for output, storage
 * management, files, and the environment.
 */

#include "system.h"

#include <stddef.h>
#include <stdlib.h>

#include "char.h"
#include "gc.h"
#include "prims.h"
#include "read.h"

const char *minim_getenv(const char *name) {
    const char *s = getenv(name);
    return s == NULL || *s == '\0' ? NULL : s;
}

static minim_obj prim_write(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_write(stdout, args[0]);
    return MINIM_VOID;
}

static minim_obj prim_display(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_display(stdout, args[0]);
    return MINIM_VOID;
}

static minim_obj prim_newline(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    putchar('\n');
    return MINIM_VOID;
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
    FILE *f = fopen(name, "r");
    free(name);
    if (f != NULL)
        fclose(f);
    return boolean(f != NULL);
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

// The forms of the file `args[0]`, as `load` reads them.
static minim_obj prim_read_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "%read-file: not a string", args[0]);
    return minim_read_file(args[0]);
}

const minim_prim_def minim_system_prims[] = {
    {"file-exists?", prim_file_exists, 1, 1},
    {"get-environment-variable", prim_get_environment_variable, 1, 1},
    {"%read-file", prim_read_file, 1, 1},
    {"write", prim_write, 1, 1},
    {"display", prim_display, 1, 1},
    {"newline", prim_newline, 0, 0},
    {"collect", prim_collect, 0, 0},
    {"collect-nursery-bytes", prim_collect_nursery_bytes, 0, 1},
    {"collect-generation-radix", prim_collect_generation_radix, 0, 1},
    {"collect-maximum-generation", prim_collect_maximum_generation, 0, 1},
    {NULL, NULL, 0, 0},
};

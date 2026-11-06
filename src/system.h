/**
 * @file
 * Interface to the operating system.
 */

#ifndef MINIM_SYSTEM_H
#define MINIM_SYSTEM_H

#include <stdio.h>

/**
 * Value of the environment variable `name`, or NULL if it is unset or empty.
 * The caller converts it, and must not free or modify it.
 */
const char *minim_getenv(const char *name);

/** The environment, `NAME=value` entries ending with NULL, as POSIX's `environ`. */
char **minim_environ(void);

/** The file `path` opened with `mode`, as fopen, or NULL; fopen_s on MSVC. */
FILE *minim_fopen(const char *path, const char *mode);

#endif

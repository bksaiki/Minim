/**
 * @file
 * Interface to the operating system.
 */

#ifndef MINIM_SYSTEM_H
#define MINIM_SYSTEM_H

/**
 * Value of the environment variable `name`, or NULL if it is unset or empty.
 * The caller converts it, and must not free or modify it.
 */
const char *minim_getenv(const char *name);

#endif

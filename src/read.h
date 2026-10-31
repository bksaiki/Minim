/**
 * @file
 * Reading from strings.  Reading from files is in minim.h.
 */

#ifndef MINIM_READ_H
#define MINIM_READ_H

#include "minim.h"

/**
 * Reads the next datum from the UTF-8 string `*s` and advances `*s` past it,
 * or returns MINIM_EOF at the end.  Raises on bad syntax, leaving `*s`
 * unchanged.
 */
minim_obj minim_read_string(const char **s);

/**
 * The forms of the file named by the string `path`, as a list.  Raises
 * `load: cannot open file` or a read error; the file is always closed.
 */
minim_obj minim_read_file(minim_obj path);

#endif

/**
 * @file
 * Reading from C strings and files; reading from ports is in minim.h.
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

/** Whether the code point `c`, or the end of input, -1, ends a token. */
bool minim_is_delimiter(int32_t c);

/**
 * The forms of the file named by the string `path`, as a list, read as if
 * after `#!fold-case` if `fold`.  Raises `load: cannot open file` or a read
 * error; the file is always closed.
 */
minim_obj minim_read_file(minim_obj path, bool fold);

#endif

/**
 * @file
 * Ports' internal interface; the public one is in minim.h.
 */

#ifndef MINIM_PORT_H
#define MINIM_PORT_H

#include "minim.h"

/** Makes the standard ports; call after minim_gc_init. */
void minim_port_init(void);

/**
 * New port with `flags`, a direction and MINIM_PORT_BINARY or not, over the
 * file `path`, closed when collected.  Raises a file error, `msg`, if it can't
 * open it.
 */
minim_obj minim_port_open_file(minim_obj path, unsigned flags, const char *msg);

/** Pushes back `c`, the last char read from the input port `p`, or -1 for its end. */
void minim_port_unread_char(minim_obj p, int32_t c);

/** Sets or clears MINIM_PORT_FOLD_CASE of the port `p`. */
void minim_port_set_fold_case(minim_obj p, bool fold);

/** Writes the `n` chars `chars`, outside the heap, to the open output port `p`. */
void minim_port_write_chars(minim_obj p, const uint32_t *chars, size_t n);

#endif

/**
 * @file
 * The prelude: the Scheme sources in `scheme/`, the library and the expander,
 * loaded from their expansion (`scheme/prelude.boot`).
 */

#ifndef MINIM_PRELUDE_H
#define MINIM_PRELUDE_H

/** Evaluates the prelude; call last in minim_init.  An error aborts. */
void minim_prelude_init(void);

#endif

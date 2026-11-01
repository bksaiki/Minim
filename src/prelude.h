/**
 * @file
 * The prelude: the expander and library procedures written in Scheme
 * (`src/prelude.scm`), loaded from its expansion (`src/prelude.boot`).
 */

#ifndef MINIM_PRELUDE_H
#define MINIM_PRELUDE_H

/** Evaluates the prelude; call last in minim_init.  An error aborts. */
void minim_prelude_init(void);

#endif

/**
 * @file
 * The prelude: library procedures (`src/prelude.scm`) and the expander
 * (`src/expander.scm`), written in Scheme and loaded from their expansion
 * (`src/prelude.boot`).
 */

#ifndef MINIM_PRELUDE_H
#define MINIM_PRELUDE_H

/** Evaluates the prelude; call last in minim_init.  An error aborts. */
void minim_prelude_init(void);

#endif

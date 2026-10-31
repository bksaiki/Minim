/**
 * @file
 * Loading the embedded prelude.
 */

#include "prelude.h"

#include "minim.h"
#include "read.h"

// The bytes of src/prelude.scm, NUL-terminated; generated at build time.
extern const unsigned char minim_prelude_scm[];

void minim_prelude_init(void) {
    const char *s = (const char *)minim_prelude_scm;
    minim_obj x;
    while ((x = minim_read_string(&s)) != MINIM_EOF)
        minim_eval(x);
}

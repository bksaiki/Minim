/**
 * @file
 * Loading the embedded prelude.
 */

#include "prelude.h"

#include "eval.h"
#include "minim.h"
#include "read.h"

// The bytes of src/prelude.boot, NUL-terminated; embedded at build time.
extern const unsigned char minim_prelude_boot[];

void minim_prelude_init(void) {
    const char *s = (const char *)minim_prelude_boot;
    minim_obj x;
    while ((x = minim_read_string(&s)) != MINIM_EOF)
        minim_eval_expanded(x);
}

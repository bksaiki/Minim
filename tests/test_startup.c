/**
 * @file
 * Tests that the environment's collector settings apply after startup, or
 * during it with MINIM_GC_STARTUP=1.
 */

#include <string.h>

#include "gc.h"
#include "minim.h"
#include "test.h"

// argv[1] is "startup" under MINIM_GC_STARTUP=1, otherwise "program".
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    minim_init();
    size_t minors = minim_gc.collections[0];
    size_t majors = minim_gc.collections[1];
    if (strcmp(argv[1], "startup") == 0) {
        // A collection at every allocation, both kinds.
        CHECK(minors > 1000);
        CHECK(majors > 100);
    } else {
        // At most one minor collection per default nursery's worth of allocation.
        CHECK(minors <= minim_gc.allocated / minim_gc_default_config().nursery_bytes);
    }

    // Either way, the program runs with the environment's settings.
    CHECK_EQ(minim_gc_get_config().nursery_bytes, 1);
    minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK(minim_gc.collections[0] + minim_gc.collections[1] > minors + majors);
    minim_shutdown();
    return test_result();
}

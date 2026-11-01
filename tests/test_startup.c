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
    minim_gc_config defaults = minim_gc_default_config();
    if (strcmp(argv[1], "startup") == 0) {
        // A collection at every allocation, of every generation.
        CHECK(minim_gc.collections[0] > 1000);
        CHECK(minim_gc.collections[defaults.max_generation] > 0);
    } else {
        // At most one collection per default nursery's worth of allocation.
        CHECK(minim_gc.collections[0] <= minim_gc.allocated / defaults.nursery_bytes);
    }

    // Either way, the program runs with the environment's settings.
    CHECK_EQ(minim_gc_get_config().nursery_bytes, 1);
    CHECK_EQ(minim_gc.scheduled, 0);
    minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK_EQ(minim_gc.scheduled, 1);
    minim_shutdown();
    return test_result();
}

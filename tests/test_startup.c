/**
 * @file
 * Tests for when the environment's collector settings apply.  Run with
 * MINIM_GC_NURSERY=1 and either argument: `program` expects startup to run
 * with the defaults, `startup` (with MINIM_GC_STARTUP=1 and MINIM_GC_RADIX=1)
 * expects a major collection at every allocation during startup.
 */

#include <string.h>

#include "gc.h"
#include "minim.h"
#include "test.h"

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    minim_init();
    size_t minors = minim_gc.minor_collections;
    size_t majors = minim_gc.major_collections;
    if (strcmp(argv[1], "startup") == 0) {
        CHECK_EQ(minors, 0);
        CHECK(majors > 1000);
    } else {
        CHECK(minors + majors < 10);
    }

    // Either way, the program runs with the environment's settings.
    CHECK_EQ(minim_gc_get_config().nursery_bytes, 1);
    minim_cons(MINIM_NIL, MINIM_NIL);
    CHECK(minim_gc.minor_collections + minim_gc.major_collections > minors + majors);
    minim_shutdown();
    return test_result();
}

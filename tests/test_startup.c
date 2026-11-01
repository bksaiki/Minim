/**
 * @file
 * Tests that the environment's collector settings apply from the start of
 * minim_init, so a stressed run stresses startup too.
 */

#include "gc.h"
#include "minim.h"
#include "test.h"

// Run with MINIM_GC_NURSERY_BYTES=1.
int main(void) {
    minim_init();
    // A collection at every allocation, of every generation.
    CHECK(minim_gc.collections[0] > 1000);
    CHECK(minim_gc.collections[minim_gc_default_config().max_generation] > 0);
    CHECK_EQ(minim_gc_get_config().nursery_bytes, 1);
    minim_shutdown();
    return test_result();
}

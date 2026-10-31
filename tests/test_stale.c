/**
 * @file
 * Tests that a stale pointer into a collected chunk stays poisoned while other
 * spare chunks are reused.
 */

#include "gc.h"
#include "heap.h"
#include "minim.h"

int main(void) {
    minim_init();
    minim_gc_config config = minim_gc_get_config();
    config.nursery_bytes = 64 * 1024 * 1024;  // no collections but the one below
    minim_gc_set_config(config);

    minim_obj p = minim_cons(minim_fixnum(1), MINIM_NIL);  // unrooted
    minim_gc_collect_minor();  // `p`'s chunk becomes the newest spare

    // Take every spare chunk, one per vector; without the quarantine, the last
    // is `p`'s.
    for (size_t n = minim_gc.spares.len; n > 0; n--)
        minim_make_vector(MINIM_CHUNK_SIZE / 2 / sizeof(minim_obj), MINIM_NIL);
    return minim_car(p) == minim_fixnum(1);  // reads the stale pair
}

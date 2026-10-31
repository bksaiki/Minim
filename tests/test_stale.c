/**
 * @file
 * A stale pointer into a collected chunk must be reported by AddressSanitizer,
 * even once every other spare chunk has been reused.  Built only with
 * MINIM_SANITIZE; the test passes when ASan reports a use after poison.
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

    // Take every spare chunk; without the quarantine, the last is `p`'s.
    size_t fill = (MINIM_CHUNK_SIZE - offsetof(minim_chunk, data)) / sizeof(minim_obj) - 1;
    for (size_t n = minim_gc.spares.len; n > 0; n--)
        minim_make_vector(fill, MINIM_NIL);
    return minim_car(p) == minim_fixnum(1);  // reads the stale pair
}

/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

void minim_boot_init() {
    mobj tc;

    // initialize globals
    init_minim();

    // initialize thread
    tc = Mthread_context();
    tc_stack_base(tc) = GC_alloc(STACK_SIZE);
    tc_stack_size(tc) = STACK_SIZE;
    tc_env(tc) = make_env();
    current_tc() = tc;
}

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
    tc_stack_base(tc) = GC_alloc(stack_size);
    tc_stack_size(tc) = stack_size;
    tc_sfp(tc) = tc_stack_base(tc);
    tc_esp(tc) = tc_sfp(tc) + tc_stack_size(tc) - stack_slop;
    tc_env(tc) = make_env();
    current_tc() = tc;
}

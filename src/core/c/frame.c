// frame.c: continuation frames

#include "../minim.h"

mobj Mcontinuation(mobj prev, mobj env, minim_thread *th) {
    mobj frame = GC_alloc(continuation_size);
    continuation_prev(frame) = prev;
    continuation_env(frame) = env;
    continuation_sfp(frame) = current_sfp(th);
    continuation_cp(frame) = current_cp(th);
    continuation_ac(frame) = current_ac(th);
    return frame;
}

mobj Mstack_segment(mobj prev, size_t size) {
    mobj sseg = GC_alloc(size);
    stack_seg_prev(sseg) = prev;
    stack_seg_size(sseg) = size;
    return sseg;
}

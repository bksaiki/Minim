// frame.c: continuation frames

#include "../minim.h"

mobj Mcontinuation(mobj prev, mobj pc, mobj env, minim_thread *th) {
    mobj o = GC_alloc(continuation_size);
    minim_type(o) = MINIM_OBJ_CONTINUATION;
    continuation_prev(o) = prev;
    continuation_pc(o) = pc;
    continuation_env(o) = env;
    continuation_sfp(o) = current_sfp(th);
    continuation_cp(o) = current_cp(th);
    continuation_ac(o) = current_ac(th);
    return o;
}

mobj Mstack_segment(mobj prev, size_t size) {
    mobj sseg = GC_alloc(size);
    stack_seg_prev(sseg) = prev;
    stack_seg_size(sseg) = size;
    return sseg;
}

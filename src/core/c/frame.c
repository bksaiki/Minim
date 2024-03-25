// frame.c: continuation frames

#include "../minim.h"

mobj Mcontinuation(mobj prev, mobj pc, mobj env, mobj tc) {
    mobj o = GC_alloc(continuation_size);
    minim_type(o) = MINIM_OBJ_CONTINUATION;
    continuation_prev(o) = prev;
    continuation_pc(o) = pc;
    continuation_env(o) = env;
    continuation_sfp(o) = tc_sfp(tc);
    continuation_cp(o) = tc_cp(tc);
    continuation_ac(o) = tc_ac(tc);
    return o;
}

mobj Mcached_stack(mobj *base, mobj prev, size_t len, mobj ret) {
    mobj o = GC_alloc(cache_stack_size);
    cache_stack_base(o) = base;
    cache_stack_prev(o) = prev;
    cache_stack_len(o) = len;
    cache_stack_ret(o) = ret;
    return o;
}

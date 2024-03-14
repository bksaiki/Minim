// frame.c: continuation frames

#include "../minim.h"

mobj Mcontinuation(mobj prev, mobj env, size_t size) {
    mobj frame = GC_alloc(continuation_size(size));
    continuation_prev(frame) = prev;
    continuation_env(frame) = env;
    continuation_len(frame) = size;
    return frame;
}

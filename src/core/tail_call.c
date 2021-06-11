#include "../gc/gc.h"
#include "tail_call.h"

static void gc_minim_tail_call_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimTailCall *call = (MinimTailCall*) ptr;
    mrk(gc, call->lam);
    mrk(gc, call->args);
}

void init_minim_tail_call(MinimTailCall **ptail, MinimLambda *lam, size_t argc, MinimObject **args)
{
    MinimTailCall *call = GC_alloc_opt(sizeof(MinimTailCall), NULL, gc_minim_tail_call_mrk);
    copy_minim_lambda(&call->lam, lam);
    call->args = GC_alloc(argc * sizeof(MinimObject*));
    call->argc = argc;
    *ptail = call;

    for (size_t i = 0; i < argc; ++i)
        call->args[i] = args[i];
}

void copy_minim_tail_call(MinimTailCall **ptail, MinimTailCall *src)
{
    MinimTailCall *call = GC_alloc_opt(sizeof(MinimTailCall), NULL, gc_minim_tail_call_mrk);
    copy_minim_lambda(&call->lam, src->lam);
    call->args = GC_alloc(src->argc * sizeof(MinimObject*));
    call->argc = src->argc;
    *ptail = call;

    for (size_t i = 0; i < src->argc; ++i)
        call->args[i] = src->args[i];
}

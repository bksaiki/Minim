#include "minimpriv.h"

void init_minim_tail_call(MinimTailCall **ptail, MinimLambda *lam, size_t argc, MinimObject **args)
{
    MinimTailCall *call = GC_alloc(sizeof(MinimTailCall));
    call->lam = lam;
    call->args = GC_alloc(argc * sizeof(MinimObject*));
    call->argc = argc;
    *ptail = call;

    for (size_t i = 0; i < argc; ++i)
        call->args[i] = args[i];
}

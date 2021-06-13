#ifndef _MINIM_TAIL_CALL_H_
#define _MINIM_TAIL_CALL_H_

#include "env.h"
#include "lambda.h"

typedef struct MinimTailCall
{
    MinimLambda *lam;
    MinimObject **args;
    size_t argc;
} MinimTailCall;

void init_minim_tail_call(MinimTailCall **ptail, MinimLambda *lam, size_t argc, MinimObject **args);
void copy_minim_tail_call(MinimTailCall **ptail, MinimTailCall *src);

#endif

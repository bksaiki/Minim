#ifndef _MINIM_SEQUENCE_H_
#define _MINIM_SEQUENCE_H_

#include "env.h"

typedef struct MinimSeq
{
    MinimObject *val, *first, *rest, *donep;
} MinimSeq;

void init_minim_seq(MinimSeq **pseq, MinimObject *init, MinimObject *first, MinimObject *rest, MinimObject *donep);
void copy_minim_seq(MinimSeq **pseq, MinimSeq *src);

#endif

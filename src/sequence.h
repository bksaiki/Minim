#ifndef _MINIM_SEQUENCE_H_
#define _MINIM_SEQUENCE_H_

#include "env.h"

typedef enum MinimSeqType
{
    MINIM_SEQ_NUM_RANGE
} MinimSeqType;

typedef struct MinimSeq
{
    void *state;
    void *end;
    MinimSeqType type;
    bool done;
} MinimSeq;

void init_minim_seq(MinimSeq **pseq, MinimSeqType type, ...);
void copy_minim_seq(MinimSeq **pseq, MinimSeq *src);
void free_minim_seq(MinimSeq *seq);

MinimObject *minim_seq_get(MinimSeq *seq);
void minim_seq_next(MinimSeq *seq);
bool minim_seq_donep(MinimSeq *seq);

/* Internals */

bool minim_sequencep(MinimObject *thing);

#endif
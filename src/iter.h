#ifndef _MINIM_ITER_H_
#define _MINIM_ITER_H_

#include "env.h"

typedef enum MinimIterType
{
    MINIM_ITER_LIST
} MinimIterType;

typedef struct MinimIter
{
    void *data;
    MinimIterType type;
    bool end;
} MinimIter;

void init_minim_iter(MinimIter **piter, MinimObject *obj);
void copy_minim_iter(MinimIter **pdest, MinimIter *src);
void free_minim_iter(MinimIter *iter);

void minim_iter_next(MinimIter *iter);
void *minim_iter_peek(MinimIter *iter);
bool minim_iter_endp(MinimIter *iter);

// Internals

bool minim_is_iterable(MinimObject *obj);
bool assert_minim_iterable(MinimObject *obj, MinimObject **res, const char *msg);

#endif
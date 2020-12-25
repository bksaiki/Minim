#ifndef _MINIM_ITER_H_
#define _MINIM_ITER_H_

#include "env.h"

typedef struct MinimIterObjs
{
    MinimObject **objs;
    int *refs;
    int count;
} MinimIterObjs;

typedef enum MinimIterType
{
    MINIM_ITER_LIST,
    MINIM_ITER_SEQ
} MinimIterType;

typedef struct MinimIter
{
    MinimIterObjs *iobjs;
    void *data;
    MinimIterType type;
    bool end;
} MinimIter;

void init_minim_iter(MinimIter **piter, MinimObject *obj);
void copy_minim_iter(MinimIter **pdest, MinimIter *src);
void free_minim_iter(MinimIter *iter);

// iter object list

void init_minim_iter_objs(MinimIterObjs **piobjs);
void free_minim_iter_objs(MinimIterObjs *iobjs);
void minim_iter_objs_add(MinimIterObjs *iobjs, MinimIter *iter);

// Internals

MinimObject *minim_iter_next(MinimObject *obj);
MinimObject *minim_iter_get(MinimObject *obj);
bool minim_iter_endp(MinimObject *obj);
bool minim_is_iterable(MinimObject *obj);

// Builtins

void env_load_module_iter(MinimEnv *env);

#endif
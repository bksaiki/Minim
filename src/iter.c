#include <stdio.h>
#include <stdlib.h>

#include "iter.h"
#include "list.h"

void free_minim_iter(MinimIter *iter)
{
    if (iter->type == MINIM_ITER_LIST)
        free_minim_object(iter->data);

    free(iter);
}

void init_minim_iter(MinimIter **piter, MinimObject *obj)
{
    MinimIter *iter;

    if (obj->type == MINIM_OBJ_PAIR && minim_listp(obj))
    {
        iter = malloc(sizeof(MinimIter));
        iter->data = obj;
        iter->end = minim_nullp(obj);
        iter->type = MINIM_ITER_LIST;
        *piter = iter;
    }
    else
    {
        printf("Object not iterable!\n");
        *piter = NULL;
    }
}

void copy_minim_iter(MinimIter **pdest, MinimIter *src)
{
    MinimIter *iter = malloc(sizeof(MinimIter));

    iter->data = src->data;
    iter->end = src->end;
    iter->type = src->type;
    *pdest = iter;
}

void *minim_iter_next(MinimIter *iter)
{
    if (iter->type == MINIM_ITER_LIST)
    {
        MinimObject *obj = iter->data;
        MinimObject *res = MINIM_CAR(obj);

        iter->data = MINIM_CDR(obj);
        iter->end = minim_nullp(iter->data);
        MINIM_CDR(obj) = NULL;
        free_minim_object(obj);

        return res;
    }

    return NULL;
}

void *minim_iter_peek(MinimIter *iter)
{
    if (iter->type == MINIM_ITER_LIST)
    {
        MinimObject *list = iter->data;
        return MINIM_CAR(list);
    }

    return NULL;
}
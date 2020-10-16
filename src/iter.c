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
        iter->type = MINIM_ITER_LIST;
        iter->end = minim_nullp(obj);
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
    iter->type = src->type;
    iter->end = src->end;
    *pdest = iter;
}

void minim_iter_next(MinimIter *iter)
{
    if (iter->type == MINIM_ITER_LIST)
    {
        MinimObject *obj = iter->data;

        iter->data = MINIM_CDR(obj);
        iter->end = !iter->data;
        MINIM_CDR(obj) = NULL;
        free_minim_object(obj);
    }
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

bool minim_iter_endp(MinimIter *iter)
{
    return iter->end;
}


bool minim_is_iterable(MinimObject *obj)
{
    if (minim_listp(obj))       return true;

    return false;
}

bool assert_minim_iterable(MinimObject *obj, MinimObject **res, const char *msg)
{
    if (!minim_is_iterable(obj))
    {
        minim_error(res, "%s", msg);
        return false;
    }

    return true;
}
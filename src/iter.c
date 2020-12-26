#include <stdio.h>
#include <stdlib.h>

#include "iter.h"
#include "list.h"
#include "sequence.h"

static void add_iter_ref(MinimIterObjs *iobjs, MinimObject* obj)
{
    for (int i = 0; i < iobjs->count; ++i)
    {
        if (iobjs->objs[i] == obj)
            ++iobjs->refs[i];
    }

    ++iobjs->count;
    iobjs->objs = realloc(iobjs->objs, iobjs->count * sizeof(MinimObject*));
    iobjs->refs = realloc(iobjs->refs, iobjs->count * sizeof(int));
    iobjs->objs[iobjs->count - 1] = obj;
    iobjs->refs[iobjs->count - 1] = 1;
}

static bool remove_iter_ref(MinimIterObjs *iobjs, MinimObject* obj)
{
    for (int i = 0; i < iobjs->count; ++i)
    {
        if (iobjs->objs[i] == obj)
        {
            --iobjs->refs[i];
            if (iobjs->refs[i] == 0)
            {
                free_minim_object(iobjs->objs[i]);
                iobjs->objs[i] = iobjs->objs[iobjs->count - 1];
                iobjs->refs[i] = iobjs->refs[iobjs->count - 1];
                
                --iobjs->count;
                iobjs->objs = realloc(iobjs->objs, iobjs->count * sizeof(MinimObject*));
                iobjs->refs = realloc(iobjs->refs, iobjs->count * sizeof(int));
            }

            return true;
        }
    }

    return false;
}

void free_minim_iter(MinimIter *iter)
{
    if (iter->iobjs)  remove_iter_ref(iter->iobjs, iter->data);
    free(iter);
}

void init_minim_iter(MinimIter **piter, MinimObject *obj)
{
    MinimIter *iter;

    if (obj->type == MINIM_OBJ_PAIR && minim_listp(obj))
    {
        iter = malloc(sizeof(MinimIter));
        iter->iobjs = NULL;
        iter->data = obj;
        iter->type = MINIM_ITER_LIST;
        iter->end = minim_nullp(obj);
        *piter = iter;
    }
    else if (obj->type == MINIM_OBJ_SEQ)
    {
        iter = malloc(sizeof(MinimIter));
        iter->iobjs = NULL;
        iter->data = obj;
        iter->type = MINIM_ITER_SEQ;
        iter->end = minim_seq_donep(obj->data);
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

    iter->iobjs = src->iobjs;
    iter->data = src->data;
    iter->type = src->type;
    iter->end = src->end;
    *pdest = iter;

    if (src->iobjs)
        add_iter_ref(iter->iobjs, src->data);
}

void init_minim_iter_objs(MinimIterObjs **piobjs)
{
    MinimIterObjs *iobjs;

    iobjs = malloc(sizeof(MinimIterObjs));
    iobjs->count = 0;
    iobjs->objs = NULL;
    iobjs->refs = NULL;
    *piobjs = iobjs;
}

void free_minim_iter_objs(MinimIterObjs *iobjs)
{
    if (iobjs->count != 0)
    {
        for (int i = 0; i < iobjs->count; ++i)
            free_minim_object(iobjs->objs[i]);

        free(iobjs->objs);
        free(iobjs->refs);
    }

    free(iobjs);
}

void minim_iter_objs_add(MinimIterObjs *iobjs, MinimIter *iter)
{
    iter->iobjs = iobjs;
    add_iter_ref(iobjs, iter->data);
}

MinimObject *minim_iter_next(MinimObject *obj)
{
    switch (obj->type)
    {
    case MINIM_OBJ_PAIR:
        if (MINIM_CDR(obj))
        {
            obj->data = MINIM_CDR(obj)->data;
        }
        else                
        {
            free_minim_object(obj);
            init_minim_object(&obj, MINIM_OBJ_PAIR, NULL, NULL);
        }
        return obj;

    case MINIM_OBJ_SEQ:
        minim_seq_next(obj->data);
        return obj;
    
    default:
        printf("Object not iterable\n");
        return NULL;
    }
}

MinimObject *minim_iter_get(MinimObject *obj)
{
    MinimObject *ref;

    switch (obj->type)
    {
    case MINIM_OBJ_PAIR:
        ref_minim_object(&ref, MINIM_CAR(obj));
        return ref;
    
    case MINIM_OBJ_SEQ:
        return minim_seq_get(obj->data);
    
    default:
        printf("Object not iterable\n");
        return NULL;
    }
}

bool minim_iter_endp(MinimObject *obj)
{
    switch (obj->type)
    {
    case MINIM_OBJ_PAIR:
        return minim_nullp(obj);

    case MINIM_OBJ_SEQ:
        return minim_seq_donep(obj->data);
    
    default:
        printf("Object not iterable\n");
        return false;
    }
}

bool minim_is_iterable(MinimObject *obj)
{
    if (minim_listp(obj))           return true;
    if (minim_sequencep(obj))       return true;

    return false;
}

// Builtins

void env_load_module_iter(MinimEnv *env)
{
    init_minim_iter_objs(&env->iobjs);
}
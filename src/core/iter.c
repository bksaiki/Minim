#include <stdio.h>
#include <stdlib.h>

#include "iter.h"
#include "list.h"
#include "sequence.h"

void init_minim_iter(MinimObject **piter, MinimObject *iterable)
{
    switch (iterable->type)
    {
    case MINIM_OBJ_PAIR:
        ref_minim_object(piter, iterable);
        break;
    
    case MINIM_OBJ_SEQ:
        if (MINIM_OBJ_OWNERP(iterable))     ref_minim_object(piter, iterable);
        else                                *piter = copy2_minim_object(iterable);
        break;
    
    default:
        printf("Object not iterable\n");
        *piter = NULL;
    }
}

MinimObject *minim_iter_next(MinimObject *obj)
{
    switch (obj->type)
    {
    case MINIM_OBJ_PAIR:
        if (MINIM_CDR(obj))
        {
            obj->u = MINIM_CDR(obj)->u;
        }
        else                
        {
            free_minim_object(obj);
            init_minim_object(&obj, MINIM_OBJ_PAIR, NULL, NULL);
        }
        return obj;

    case MINIM_OBJ_SEQ:
        minim_seq_next(obj->u.ptrs.p1);
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
        return minim_seq_get(obj->u.ptrs.p1);
    
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
        return minim_seq_donep(obj->u.ptrs.p1);
    
    default:
        printf("Object not iterable\n");
        return false;
    }
}

bool minim_iterablep(MinimObject *obj)
{
    if (minim_listp(obj))           return true;
    if (minim_sequencep(obj))       return true;

    return false;
}
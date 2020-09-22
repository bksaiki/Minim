#include <stdio.h>
#include <stdlib.h>
#include "object.h"

static char* minim_object_str(MinimObject* obj)
{
    char* ret = malloc(256 * sizeof(char));

    if (obj->type == MINIM_OBJ_NUM)
    {
        sprintf(ret, "%d", *((int*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_SYM)
    {
        sprintf(ret, "%s", ((char*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        sprintf(ret, "%s", ((char*)obj->data));
    }
    else
    {
        sprintf(ret, "<error!>");
    }

    return ret;
}

int print_minim_object(MinimObject* obj)
{
    if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject** arr = ((MinimObject**) obj->data);
        printf("(%s . %s)", minim_object_str(arr[0]), minim_object_str(arr[1]));
    }
    else
    {
        printf("%s", minim_object_str(obj));
    }

    return 0;
}
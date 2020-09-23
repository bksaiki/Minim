#include <stdarg.h>
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
    else if (obj->type == MINIM_OBJ_SYM || obj->type == MINIM_OBJ_ERR)
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

// Visible functions

MinimObject *construct_minim_object(MinimObjectType type, ...)
{
    MinimObject *obj = malloc(sizeof(MinimObject));
    va_list rest;

    va_start(rest, type);
    obj->type = type;
    if (type == MINIM_OBJ_NUM)
    {
        int *v = malloc(sizeof(int));
        *v = va_arg(rest, int);
        obj->data = v;
    }
    else if (type == MINIM_OBJ_SYM)
    {
        obj->data = va_arg(rest, char*);
    }
    else if (type == MINIM_OBJ_ERR)
    {
        obj->data = va_arg(rest, char*);
    }
    else if (type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = malloc(2 * sizeof(MinimObject**));
        pair[0] = va_arg(rest, MinimObject*);
        pair[1] = va_arg(rest, MinimObject*);
        obj->data = pair;
    }
    else
    {
        printf("Unknown object type\n");
    }

    va_end(rest);
    return obj;
}

void free_minim_object(MinimObject *obj)
{
    if (obj == NULL)
        return;

    if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject** pdata = (MinimObject**)obj->data;
        if (pdata[0])   free_minim_object(pdata[0]);
        if (pdata[1])   free_minim_object(pdata[1]);
    }
    
    if (obj->data)   free(obj->data);
    free(obj);
}

int print_minim_object(MinimObject *obj)
{
    if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject **arr = ((MinimObject**) obj->data);
        char *car = minim_object_str(arr[0]);
        char *cdr = minim_object_str(arr[1]);

        printf("(%s . %s)", car, cdr);
        free(car);
        free(cdr);
    }
    else
    {
        char *str = minim_object_str(obj);
        printf("%s", str);
        free(str);
    }

    return 0;
}

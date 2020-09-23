#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object.h"

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
    else if (type == MINIM_OBJ_SYM || type == MINIM_OBJ_ERR)
    {
        char *dest, *src = va_arg(rest, char*);
        int len = strlen(src);
        
        dest = calloc(sizeof(char), len + 1);
        strncpy(dest, src, len);
        obj->data = dest;
    }
    else if (type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = malloc(2 * sizeof(MinimObject**));
        pair[0] = copy_minim_object(va_arg(rest, MinimObject*));
        pair[1] = copy_minim_object(va_arg(rest, MinimObject*));
        obj->data = pair;
    }
    else
    {
        printf("Unknown object type\n");
        obj = NULL;
    }

    va_end(rest);
    return obj;
}

MinimObject *copy_minim_object(MinimObject* obj)
{
    MinimObject *cp = malloc(sizeof(MinimObject));

    cp->type = obj->type;
    if (obj->type == MINIM_OBJ_NUM)
    {
        int *v = malloc(sizeof(int));
        *v = *((int*) obj->data);
        cp->data = v;
    }
    else if (obj->type == MINIM_OBJ_SYM || obj->type == MINIM_OBJ_ERR)
    {
        char *dest, *src = ((char*) obj->data);
        int len = strlen(src);

        dest = calloc(sizeof(char), len + 1);
        strncpy(dest, src, len);
        cp->data = dest;
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject** src = (MinimObject**) obj->data;
        MinimObject** dest = malloc(2 * sizeof(MinimObject*));

        dest[0] = copy_minim_object(src[0]);
        dest[1] = copy_minim_object(src[1]);
        cp->data = dest;
    }
    else
    {
        printf("Unknown object type\n");
        cp = NULL;
    }

    return cp;
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
    if (obj->type == MINIM_OBJ_NUM)
    {
        printf("%d", *((int*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_SYM || obj->type == MINIM_OBJ_ERR)
    {
        printf("%s", ((char*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject **arr = ((MinimObject**) obj->data);

        printf("(");
        print_minim_object(arr[0]);
        printf(" . ");
        print_minim_object(arr[1]);
        printf(")");
    }
    else
    {
        printf("<error!>");
        return 1;
    }

    return 0;
}

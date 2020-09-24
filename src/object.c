#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "util.h"

static void free_minim_object_proc(const void *thing)
{
    MinimObject **obj = ((MinimObject**) thing);
    free_minim_object(*obj);
}

// Visible functions

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...)
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
        copy_minim_object(&pair[0], va_arg(rest, MinimObject*));
        copy_minim_object(&pair[1], va_arg(rest, MinimObject*));
        obj->data = pair;
    }
    else if (type == MINIM_OBJ_FUNC)
    {
        obj->data = va_arg(rest, MinimBuiltin);
    }
    else
    {
        printf("Unknown object type\n");
        obj = NULL;
    }

    va_end(rest);
    *pobj = obj;
}

void copy_minim_object(MinimObject **pobj, MinimObject *src)
{
    MinimObject *obj = malloc(sizeof(MinimObject));

    obj->type = src->type;
    if (src->type == MINIM_OBJ_NUM)
    {
        int *v = malloc(sizeof(int));
        *v = *((int*) src->data);
        obj->data = v;
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_ERR)
    {
        char *dest, *str = ((char*) src->data);
        int len = strlen(str);

        dest = calloc(sizeof(char), len + 1);
        strncpy(dest, str, len);
        obj->data = dest;
    }
    else if (src->type == MINIM_OBJ_PAIR)
    {
        MinimObject** cell = ((MinimObject**) src->data);
        MinimObject** dest = malloc(2 * sizeof(MinimObject*));

        copy_minim_object(&dest[0], cell[0]);
        copy_minim_object(&dest[1], cell[1]);
        obj->data = dest;
    }
    else if (src->type == MINIM_OBJ_FUNC)
    {
        obj = src->data;   // no copy
    }
    else
    {
        printf("Unknown object type\n");
        obj = NULL;
    }

    *pobj = obj;
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
    
    if (obj->data && obj->type != MINIM_OBJ_FUNC)
        free(obj->data);

    free(obj);
}

void free_minim_objects(int count, MinimObject **objs)
{
    for_each(objs, count, sizeof(MinimObject*), free_minim_object_proc);
    free(objs);
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
    else if (obj->type == MINIM_OBJ_FUNC)
    {
        printf("<function: %s>", ((char*)obj->data));
    }
    else
    {
        printf("<Unknown type>");
        return 1;
    }

    return 0;
}

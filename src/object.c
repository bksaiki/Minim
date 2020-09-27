#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "lambda.h"
#include "object.h"
#include "parser.h"
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
    if (type == MINIM_OBJ_VOID)
    {
        obj->data = NULL;
    }
    else if (type == MINIM_OBJ_BOOL || type == MINIM_OBJ_NUM)
    {
        int *v = malloc(sizeof(int));
        *v = va_arg(rest, int);
        obj->data = v;
    }
    else if (type == MINIM_OBJ_SYM || type == MINIM_OBJ_ERR)
    {
        char *dest, *src = va_arg(rest, char*);
        dest = malloc((strlen(src) + 1) * sizeof(char));
        strcpy(dest, src);
        obj->data = dest;
    }
    else if (type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = malloc(2 * sizeof(MinimObject**));
        pair[0] = va_arg(rest, MinimObject*);
        pair[1] = va_arg(rest, MinimObject*);
        obj->data = pair;
    }
    else if (type == MINIM_OBJ_FUNC || type == MINIM_OBJ_SYNTAX)
    {
        obj->data = va_arg(rest, MinimBuiltin);
    }
    else if (type == MINIM_OBJ_CLOSURE)
    {
        obj->data = va_arg(rest, MinimLambda*);
    }   
    else if (type == MINIM_OBJ_AST)
    {
        obj->data = va_arg(rest, MinimAstNode*);
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
    MinimObject *obj;
    
    if (!src)
    {
        *pobj = NULL;
        return;
    }

    obj = malloc(sizeof(MinimObject));
    obj->type = src->type;

    if (src->type == MINIM_OBJ_BOOL || src->type == MINIM_OBJ_NUM)
    {
        int *v = malloc(sizeof(int));
        *v = *((int*) src->data);
        obj->data = v;
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_ERR)
    {
        char *dest, *str = ((char*) src->data);
        dest = malloc((strlen(str) + 1) * sizeof(char));
        strcpy(dest, str);      
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
    else if (src->type == MINIM_OBJ_VOID || src->type == MINIM_OBJ_FUNC ||
             src->type == MINIM_OBJ_SYNTAX)
    {
        obj->data = src->data;   // no copy
    }
    else if (src->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam = obj->data;
        copy_minim_lambda(&lam, src->data);
        obj->data = lam;
    }
    else if (src->type == MINIM_OBJ_AST)
    {
        MinimAstNode *node;
        copy_ast(&node, src->data);
        obj->data = node;
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
    
    if (obj->data && obj->type != MINIM_OBJ_FUNC && obj->type != MINIM_OBJ_SYNTAX)
    {
        if (obj->type == MINIM_OBJ_AST)             free_ast(obj->data);
        else if (obj->type == MINIM_OBJ_CLOSURE)    free_minim_lambda(obj->data);
        else                                        free(obj->data);
    }

    free(obj);
}

void free_minim_objects(int count, MinimObject **objs)
{
    for_each(objs, count, sizeof(MinimObject*), free_minim_object_proc);
    free(objs);
}

void minim_error(MinimObject **pobj, const char* format, ...)
{
    MinimObject *obj;
    char buffer[1024];
    va_list rest;
    
    va_start(rest, format);
    vsnprintf(buffer, 1024, format, rest);
    init_minim_object(&obj, MINIM_OBJ_ERR, buffer);
    va_end(rest);

    *pobj = obj;
}
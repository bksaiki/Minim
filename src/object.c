#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "object.h"
#include "parser.h"
#include "util.h"

static int print_object_port(MinimEnv *env, MinimObject *obj, FILE *stream, bool quote);

static void free_minim_object_proc(const void *thing)
{
    MinimObject **obj = ((MinimObject**) thing);
    free_minim_object(*obj);
}

static void print_list(MinimEnv *env, MinimObject *obj, FILE *stream, bool quote)
{
    MinimObject** pair = ((MinimObject**) obj->data);

    if (pair[0])
        print_object_port(env, pair[0], stream, true);

    if (pair[1])
    {
        fprintf(stream, " ");
        print_list(env, pair[1], stream, true);
    }
    else
    {
        fprintf(stream, ")");
    }
}

static int print_object_port(MinimEnv *env, MinimObject *obj, FILE *stream, bool quote)
{
    if (obj->type == MINIM_OBJ_VOID)
    {
        fprintf(stream, "<void>");
    }
    else if (obj->type == MINIM_OBJ_BOOL)
    {
        fprintf(stream, ((*((int*) obj->data)) ? "true" : "false"));
    }
    else if (obj->type == MINIM_OBJ_NUM)
    {
        fprintf(stream, "%d", *((int*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_SYM)
    {
        if (quote)  fprintf(stream, "%s", ((char*) obj->data));
        else        fprintf(stream, "'%s", ((char*) obj->data));
    }
    else if (obj->type == MINIM_OBJ_ERR)
    {
        fprintf(stream, "%s", ((char*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = ((MinimObject**) obj->data);
        if (!pair[0] || !pair[1] ||
            pair[0]->type == MINIM_OBJ_PAIR || pair[1]->type == MINIM_OBJ_PAIR)
        {
            if (quote)  fprintf(stream, "(");
            else        fprintf(stream, "'(");
            print_list(env, obj, stream, true);
        }
        else
        {
            fprintf(stream, "'(");
            print_object_port(env, pair[0], stream, true);
            fprintf(stream, " . ");
            print_object_port(env, pair[1], stream, true);
            fprintf(stream, ")");
        }
    }
    else if (obj->type == MINIM_OBJ_FUNC)
    {
        const char *str = env_peek_key(env, obj);
        if (str)    fprintf(stream, "<function: %s>", str);
        else        fprintf(stream, "<function: ?>");
    }
    else
    {
        fprintf(stream, "<Unknown type>");
        return 1;
    }

    return 0;
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
        if (obj->type == MINIM_OBJ_AST)         free_ast(obj->data);
        else                                    free(obj->data);
    }

    free(obj);
}

void free_minim_objects(int count, MinimObject **objs)
{
    for_each(objs, count, sizeof(MinimObject*), free_minim_object_proc);
    free(objs);
}

int print_minim_object(MinimEnv *env, MinimObject *obj)
{
    return print_object_port(env, obj, stdout, false);
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
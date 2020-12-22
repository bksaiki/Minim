#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "iter.h"
#include "hash.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "object.h"
#include "parser.h"
#include "sequence.h"
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
    else if (type == MINIM_OBJ_BOOL)
    {
        int *v = malloc(sizeof(int));
        *v = va_arg(rest, int);
        obj->data = v;
    }
    else if (type == MINIM_OBJ_NUM)
    {
        obj->data = va_arg(rest, MinimNumber*);
    }
    else if (type == MINIM_OBJ_SYM || type == MINIM_OBJ_ERR)
    {
        char *dest, *src = va_arg(rest, char*);
        dest = malloc((strlen(src) + 1) * sizeof(char));
        strcpy(dest, src);
        obj->data = dest;
    }
    else if (type == MINIM_OBJ_STRING)
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
        obj->data = va_arg(rest, MinimAst*);
    }
    else if (type == MINIM_OBJ_ITER)
    {
        obj->data = va_arg(rest, MinimIter*);
    }
    else if (type == MINIM_OBJ_SEQ)
    {
        obj->data = va_arg(rest, MinimSeq*);
    }
    else if (type == MINIM_OBJ_HASH)
    {
        obj->data = va_arg(rest, MinimHash*);
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

    if (src->type == MINIM_OBJ_BOOL)
    {
        int *v = malloc(sizeof(int));
        *v = *((int*) src->data);
        obj->data = v;
    }
    else if (src->type == MINIM_OBJ_NUM)
    {
        MinimNumber *num;
        copy_minim_number(&num, src->data);
        obj->data = num;
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_ERR || src->type == MINIM_OBJ_STRING)
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
        MinimLambda *lam;

        copy_minim_lambda(&lam, src->data);
        obj->data = lam;
    }
    else if (src->type == MINIM_OBJ_AST)
    {
        MinimAst *node;
        copy_ast(&node, src->data);
        obj->data = node;
    }
    else if (src->type == MINIM_OBJ_ITER)
    {
        MinimIter *iter;
        copy_minim_iter(&iter, src->data);
        obj->data = iter;
    }
    else if (src->type == MINIM_OBJ_SEQ)
    {
        MinimSeq *seq;
        copy_minim_seq(&seq, src->data);
        obj->data = seq;
    }
    else if (src->type == MINIM_OBJ_HASH)
    {
        MinimHash *ht;
        copy_minim_hash_table(&ht, src->data);
        obj->data = ht;
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
    
    if (obj->data &&
        obj->type != MINIM_OBJ_FUNC && obj->type != MINIM_OBJ_SYNTAX && // function pointer
        obj->type != MINIM_OBJ_AST) // release ast
    {
        if (obj->type == MINIM_OBJ_CLOSURE)         free_minim_lambda(obj->data);
        else if (obj->type == MINIM_OBJ_NUM)        free_minim_number(obj->data);
        else if (obj->type == MINIM_OBJ_ITER)       free_minim_iter(obj->data);
        else if (obj->type == MINIM_OBJ_SEQ)        free_minim_seq(obj->data);
        else if (obj->type == MINIM_OBJ_HASH)       free_minim_hash_table(obj->data);
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

bool minim_equalp(MinimObject *a, MinimObject *b)
{
    if (a->type != b->type)
        return false;

    switch (a->type)
    {
    case MINIM_OBJ_VOID:
        return true;

    case MINIM_OBJ_BOOL:
        return (*((int*) a->data) == *((int*) b->data));

    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
    case MINIM_OBJ_ERR:
        return strcmp(a->data, b->data) == 0;

    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
        return a->data == b->data;

    case MINIM_OBJ_NUM:
        return minim_number_cmp(a->data, b->data) == 0;

    case MINIM_OBJ_PAIR:
        return minim_cons_eqp(a, b);
    
    /*
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_AST:
    case MINIM_OBJ_ITER:
    case MINIM_OBJ_SEQ:
    */
    default:
        return false;
    }
 }

Buffer* minim_obj_to_bytes(MinimObject *obj)
{
    Buffer* bf;
    
    init_buffer(&bf);
    switch (obj->type)
    {
    case MINIM_OBJ_VOID:
        writei_buffer(bf, 0);
        break;

    case MINIM_OBJ_BOOL:
        writei_buffer(bf, (*((int*)obj->data) ? 1 : 2));
        break;
    
    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
        write_buffer(bf, obj->data, sizeof(void*));
        break;

    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
    case MINIM_OBJ_ERR:
        writes_buffer(bf, obj->data);
        break;

    /*
    case MINIM_OBJ_NUM:
    case MINIM_OBJ_PAIR:
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_AST:
    case MINIM_OBJ_ITER:
    case MINIM_OBJ_SEQ:
    */
    default:
        write_buffer(bf, obj->data, sizeof(void*));
        break;
    }

    return bf;
}
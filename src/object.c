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
#include "vector.h"

// Visible functions

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);

    obj = malloc(sizeof(MinimObject));
    obj->type = type;
    obj->flags = MINIM_OBJ_OWNER;

    if (type == MINIM_OBJ_VOID)
    {
        obj->data = NULL;
    }
    else if (type == MINIM_OBJ_BOOL)
    {
        obj->si = va_arg(rest, int);
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
    else if (type == MINIM_OBJ_SEQ)
    {
        obj->data = va_arg(rest, MinimSeq*);
    }
    else if (type == MINIM_OBJ_HASH)
    {
        obj->data = va_arg(rest, MinimHash*);
    }
    else if (type == MINIM_OBJ_VECTOR)
    {
        obj->data = va_arg(rest, MinimVector*);
    }
    else
    {
        printf("Unknown object type\n");
        obj = NULL;
    }

    va_end(rest);
    *pobj = obj;
}

static void ref_minim_object_h(MinimObject *dest, MinimObject *src)
{
    switch (src->type)
    {
    case MINIM_OBJ_BOOL:
        dest->si = src->si;
        break;

    case MINIM_OBJ_NUM:
    case MINIM_OBJ_SYM:
    case MINIM_OBJ_ERR:
    case MINIM_OBJ_STRING:
    case MINIM_OBJ_PAIR:
    case MINIM_OBJ_VOID:
    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_AST:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_HASH:
        dest->data = src->data;
        break;
    
    default:
        printf("Unknown object type\n");
        break;
    }
}

void copy_minim_object_h(MinimObject *dest, MinimObject *src)
{
    if (src->type == MINIM_OBJ_BOOL)
    {
        dest->si = src->si;
    }
    else if (src->type == MINIM_OBJ_NUM)
    {
        MinimNumber *num;
        copy_minim_number(&num, src->data);
        dest->data = num;
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_ERR || src->type == MINIM_OBJ_STRING)
    {
        char *str = ((char*) src->data);
        dest->data = malloc((strlen(str) + 1) * sizeof(char));
        strcpy(dest->data, str);
    }
    else if (src->type == MINIM_OBJ_PAIR)
    {
        MinimObject** cell = ((MinimObject**) src->data);
        MinimObject** cp = malloc(2 * sizeof(MinimObject*));

        if (cell[0]) copy_minim_object(&cp[0], cell[0]);
        else         cp[0] = NULL;

        if (cell[1]) copy_minim_object(&cp[1], cell[1]);
        else         cp[1] = NULL;

        dest->data = cp;
    }
    else if (src->type == MINIM_OBJ_VOID || src->type == MINIM_OBJ_FUNC ||
                src->type == MINIM_OBJ_SYNTAX)
    {
        dest->data = src->data;   // no copy
    }
    else if (src->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam;

        copy_minim_lambda(&lam, src->data);
        dest->data = lam;
    }
    else if (src->type == MINIM_OBJ_AST)
    {
        MinimAst *node;
        copy_ast(&node, src->data);
        dest->data = node;
    }
    else if (src->type == MINIM_OBJ_SEQ)
    {
        MinimSeq *seq;
        copy_minim_seq(&seq, src->data);
        dest->data = seq;
    }
    else if (src->type == MINIM_OBJ_HASH)
    {
        MinimHash *ht;
        copy_minim_hash_table(&ht, src->data);
        dest->data = ht;
    }
    else if (src->type == MINIM_OBJ_VECTOR)
    {
        MinimVector *vec;
        copy_minim_vector(&vec, src->data);
        dest->data = vec;
    }
    else
    {
        printf("Unknown object type\n");
        dest = NULL;
    }   
}

void copy_minim_object(MinimObject **pobj, MinimObject *src)
{
    MinimObject *obj = malloc(sizeof(MinimObject));
    obj->type = src->type;
    obj->flags = src->flags;
    *pobj = obj;

    if (MINIM_OBJ_OWNERP(obj))  copy_minim_object_h(obj, src);
    else                        ref_minim_object_h(obj, src);
}

void ref_minim_object(MinimObject **pobj, MinimObject *src)
{
    MinimObject *obj = malloc(sizeof(MinimObject));
    obj->type = src->type;
    obj->flags = (MINIM_OBJ_OWNERP(src) ? src->flags ^ MINIM_OBJ_OWNER : src->flags);
    *pobj = obj;

    ref_minim_object_h(obj, src);
}

void free_minim_object(MinimObject *obj)
{
    if (MINIM_OBJ_OWNERP(obj))
    {
        if (obj->type == MINIM_OBJ_PAIR)
        {
            MinimObject** pdata = (MinimObject**)obj->data;
            if (pdata[0])   free_minim_object(pdata[0]);
            if (pdata[1])   free_minim_object(pdata[1]);
        }
        
        if (obj->data &&
            obj->type != MINIM_OBJ_BOOL &&
            obj->type != MINIM_OBJ_FUNC && obj->type != MINIM_OBJ_SYNTAX && // function pointer
            obj->type != MINIM_OBJ_AST) // release ast
        {
            if (obj->type == MINIM_OBJ_CLOSURE)         free_minim_lambda(obj->data);
            else if (obj->type == MINIM_OBJ_NUM)        free_minim_number(obj->data);
            else if (obj->type == MINIM_OBJ_SEQ)        free_minim_seq(obj->data);
            else if (obj->type == MINIM_OBJ_HASH)       free_minim_hash_table(obj->data);
            else if (obj->type == MINIM_OBJ_VECTOR)     free_minim_vector(obj->data);
            else                                        free(obj->data);
        }
    }

    free(obj);
}

void free_minim_objects(MinimObject **objs, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (objs[i]) free_minim_object(objs[i]);
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
    
    case MINIM_OBJ_CLOSURE:
        return a->data == b->data;
    
    case MINIM_OBJ_AST:
        return ast_equalp(a->data, b->data);
    
    /*
    case MINIM_OBJ_SEQ:
    */
    default:
        return false;
    }
}

MinimObject *fresh_minim_object(MinimObject *src)
{
    MinimObject *cp;

    if (MINIM_OBJ_OWNERP(src))
        return src;

    cp = malloc(sizeof(MinimObject));
    cp->type = src->type;
    cp->flags = src->flags | MINIM_OBJ_OWNER;
    copy_minim_object_h(cp, src);

    return cp;
}

MinimObject *copy2_minim_object(MinimObject *src)
{
    MinimObject *cp = malloc(sizeof(MinimObject));
    cp->type = src->type;
    cp->flags = src->flags | MINIM_OBJ_OWNER;
    copy_minim_object_h(cp, src);

    return cp;
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
        writei_buffer(bf, (*((int*)obj->data) ? 0 : 1));
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

    case MINIM_OBJ_PAIR:
        minim_cons_to_bytes(obj, bf);
        break;

    case MINIM_OBJ_NUM:
        minim_number_to_bytes(obj, bf);
        break;

    case MINIM_OBJ_AST:
        ast_dump_in_buffer(obj->data, bf);
        break;

    case MINIM_OBJ_CLOSURE:
        minim_lambda_to_buffer(obj->data, bf);
        break;

    /*
    case MINIM_OBJ_SEQ:
    */
    default:
        write_buffer(bf, obj->data, sizeof(void*));
        break;
    }

    return bf;
}
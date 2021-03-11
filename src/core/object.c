#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "error.h"
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

void initv_minim_object(MinimObject **pobj, MinimObjectType type, va_list vargs)
{
    MinimObject *obj;

    obj = malloc(sizeof(MinimObject));
    obj->type = type;
    obj->flags = MINIM_OBJ_OWNER;

    // if (type == MINIM_OBJ_VOID)
    if (type == MINIM_OBJ_BOOL)
    {
        obj->u.ints.i1 = va_arg(vargs, int);
    }
    else if (type == MINIM_OBJ_NUM)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimNumber*);
    }
    else if (type == MINIM_OBJ_SYM)
    {
        char *dest, *src = va_arg(vargs, char*);
        dest = malloc((strlen(src) + 1) * sizeof(char));
        strcpy(dest, src);
        obj->u.str.str = dest;
    }
    else if (type == MINIM_OBJ_ERR)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimError*);
    }
    else if (type == MINIM_OBJ_STRING)
    {
        obj->u.str.str = va_arg(vargs, char*);
    }
    else if (type == MINIM_OBJ_PAIR)
    {
        obj->u.pair.car = va_arg(vargs, MinimObject*);
        obj->u.pair.cdr = va_arg(vargs, MinimObject*);
    }
    else if (type == MINIM_OBJ_FUNC || type == MINIM_OBJ_SYNTAX)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimBuiltin);
    }
    else if (type == MINIM_OBJ_CLOSURE)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimLambda*);
    }   
    else if (type == MINIM_OBJ_AST)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimAst*);
    }
    else if (type == MINIM_OBJ_SEQ)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimSeq*);
    }
    else if (type == MINIM_OBJ_HASH)
    {
        obj->u.ptrs.p1 = va_arg(vargs, MinimHash*);
    }
    else if (type == MINIM_OBJ_VECTOR)
    {
        obj->u.vec.arr = va_arg(vargs, MinimVector*);
        obj->u.vec.len = va_arg(vargs, size_t);
    }

    *pobj = obj;
}

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...)
{
    va_list rest;

    va_start(rest, type);
    initv_minim_object(pobj, type, rest);
    va_end(rest);
}

static void ref_minim_object_h(MinimObject *dest, MinimObject *src)
{
    switch (src->type)
    {
    case MINIM_OBJ_BOOL:
        dest->u = src->u;
        dest->flags |= MINIM_OBJ_OWNER; // override
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
    case MINIM_OBJ_VECTOR:
        dest->u = src->u;
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
        dest->u.ints.i1 = dest->u.ints.i2;
    }
    else if (src->type == MINIM_OBJ_NUM)
    {
        MinimNumber *num;
        copy_minim_number(&num, src->u.ptrs.p1);
        dest->u.ptrs.p1 = num;
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_STRING)
    {
        char *str = ((char*) src->u.str.str);
        dest->u.str.str = malloc((strlen(str) + 1) * sizeof(char));
        strcpy(dest->u.str.str, str);
    }
    else if (src->type == MINIM_OBJ_ERR)
    {
        MinimError *err;

        copy_minim_error(&err, src->u.ptrs.p1);
        dest->u.ptrs.p1 = err;
    }
    else if (src->type == MINIM_OBJ_PAIR)
    {
        if (src->u.pair.car)    copy_minim_object(&dest->u.pair.car, src->u.pair.car);
        else                    dest->u.pair.car = NULL;

        if (src->u.pair.cdr)    copy_minim_object(&dest->u.pair.cdr, src->u.pair.cdr);
        else                    dest->u.pair.cdr = NULL;
    }
    else if (src->type == MINIM_OBJ_VOID || src->type == MINIM_OBJ_FUNC ||
                src->type == MINIM_OBJ_SYNTAX)
    {
        dest->u.ptrs.p1 = src->u.ptrs.p1;   // no copy
    }
    else if (src->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam;

        copy_minim_lambda(&lam, src->u.ptrs.p1);
        dest->u.ptrs.p1 = lam;
    }
    else if (src->type == MINIM_OBJ_AST)
    {
        MinimAst *node;
        copy_ast(&node, src->u.ptrs.p1);
        dest->u.ptrs.p1 = node;
    }
    else if (src->type == MINIM_OBJ_SEQ)
    {
        MinimSeq *seq;
        copy_minim_seq(&seq, src->u.ptrs.p1);
        dest->u.ptrs.p1 = seq;
    }
    else if (src->type == MINIM_OBJ_HASH)
    {
        MinimHash *ht;
        copy_minim_hash_table(&ht, src->u.ptrs.p1);
        dest->u.ptrs.p1 = ht;
    }
    else if (src->type == MINIM_OBJ_VECTOR)
    {
        MinimVector *vec;
        copy_minim_vector(&vec, src->u.vec.arr);
        dest->u.vec.arr = vec;
        dest->u.vec.len = src->u.vec.len;
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
            if (obj->u.pair.car) free_minim_object(obj->u.pair.car);
            if (obj->u.pair.cdr) free_minim_object(obj->u.pair.cdr);
        }
        else if (obj->type == MINIM_OBJ_CLOSURE)    free_minim_lambda(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_NUM)        free_minim_number(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_SEQ)        free_minim_seq(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_HASH)       free_minim_hash_table(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_VECTOR)     free_minim_vector(obj->u.vec.arr);
        else if (obj->type == MINIM_OBJ_ERR)        free_minim_error(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_STRING)     free(obj->u.str.str);
        else if (obj->type == MINIM_OBJ_STRING)     free(obj->u.str.str);
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
    MinimError *err;
    char buffer[1024];
    va_list rest;
    
    va_start(rest, format);
    vsnprintf(buffer, 1024, format, rest);
    init_minim_error(&err, buffer, NULL);
    init_minim_object(&obj, MINIM_OBJ_ERR, err);
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
        return (*((int*) a->u.ints.i1) == *((int*) b->u.ints.i2));

    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
        return strcmp(a->u.str.str, b->u.str.str) == 0;

    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
        return a->u.ptrs.p1 == b->u.ptrs.p1;

    case MINIM_OBJ_NUM:
        return minim_number_cmp(a->u.ptrs.p1, b->u.ptrs.p1) == 0;

    case MINIM_OBJ_PAIR:
        return minim_cons_eqp(a, b);
    
    case MINIM_OBJ_CLOSURE:
        return a->u.ptrs.p1 == b->u.ptrs.p1;
    
    case MINIM_OBJ_AST:
        return ast_equalp(a->u.ptrs.p1, b->u.ptrs.p1);

    case MINIM_OBJ_VECTOR:
        return minim_vector_equalp(a->u.vec.arr, b->u.vec.arr);
    
    /*
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_ERR:
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
        writei_buffer(bf, (*((int*)obj->u.ints.i1) ? 0 : 1));
        break;
    
    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
        write_buffer(bf, obj->u.ptrs.p1, sizeof(void*));
        break;

    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
        writes_buffer(bf, obj->u.ptrs.p1);
        break;

    case MINIM_OBJ_PAIR:
        minim_cons_to_bytes(obj, bf);
        break;

    case MINIM_OBJ_NUM:
        minim_number_to_bytes(obj, bf);
        break;

    case MINIM_OBJ_AST:
        ast_dump_in_buffer(obj->u.ptrs.p1, bf);
        break;

    case MINIM_OBJ_CLOSURE:
        minim_lambda_to_buffer(obj->u.ptrs.p1, bf);
        break;

    case MINIM_OBJ_VECTOR:
        minim_vector_bytes(obj->u.vec.arr, bf);
        break;

    /*
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_ERR:
    */
    default:
        write_buffer(bf, obj->u.pair.car, sizeof(MinimObject*));
        write_buffer(bf, obj->u.pair.cdr, sizeof(MinimObject*));
        break;
    }

    return bf;
}
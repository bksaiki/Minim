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
    if (type == MINIM_OBJ_EXIT)
    {
        obj->u.ints.i1 = va_arg(vargs, int);
    }
    else if (type == MINIM_OBJ_BOOL)
    {
        obj->u.ints.i1 = va_arg(vargs, int);
    }
    else if (type == MINIM_OBJ_EXACT)
    {
        obj->u.ptrs.p1 = va_arg(vargs, mpq_ptr);
    }
    else if (type == MINIM_OBJ_INEXACT)
    {
        obj->u.fls.f1 = va_arg(vargs, double);
    }
    else if (type == MINIM_OBJ_SYM)
    {
        char *src = va_arg(vargs, char*);
        obj->u.str.str = malloc((strlen(src) + 1) * sizeof(char));
        strcpy(obj->u.str.str, src);
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
        obj->u.ptrs.p1 = va_arg(vargs, SyntaxNode*);
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
        obj->u.vec.arr = va_arg(vargs, MinimObject**);
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
    case MINIM_OBJ_EXIT:
        dest->u = src->u;
        dest->flags |= MINIM_OBJ_OWNER; // override
        break;

    default:
        dest->u = src->u;
        break;
    }
}

void copy_minim_object_h(MinimObject *dest, MinimObject *src)
{
    if (src->type == MINIM_OBJ_BOOL || src->type == MINIM_OBJ_EXIT)
    {
        dest->u.ints.i1 = dest->u.ints.i2;
    }
    else if (MINIM_OBJ_EXACTP(src))
    {
        mpq_ptr num = malloc(sizeof(__mpq_struct));

        mpq_init(num);
        mpq_set(num, MINIM_EXACT(src));
        dest->u.ptrs.p1 = num;
    }
    else if (MINIM_OBJ_INEXACTP(src))
    {
        MINIM_INEXACT(dest) = MINIM_INEXACT(src);
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
        SyntaxNode *node;
        copy_syntax_node(&node, src->u.ptrs.p1);
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
        dest->u.vec.arr = malloc(src->u.vec.len * sizeof(MinimObject*));
        dest->u.vec.len = src->u.vec.len;
        for (size_t i = 0; i < src->u.vec.len; ++i)
            dest->u.vec.arr[i] = copy2_minim_object(src->u.vec.arr[i]);
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
        else if (obj->type == MINIM_OBJ_VECTOR)
        {
            for (size_t i = 0; i < obj->u.vec.len; ++i)
                free_minim_object(obj->u.vec.arr[i]);
            free(obj->u.vec.arr);
        }
        else if (obj->type == MINIM_OBJ_EXACT)
        {
            mpq_clear(MINIM_EXACT(obj));
            free(MINIM_EXACT(obj));
        }
        else if (obj->type == MINIM_OBJ_CLOSURE)    free_minim_lambda(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_SEQ)        free_minim_seq(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_HASH)       free_minim_hash_table(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_ERR)        free_minim_error(obj->u.ptrs.p1);
        else if (obj->type == MINIM_OBJ_STRING)     free(obj->u.str.str);
        else if (obj->type == MINIM_OBJ_SYM)        free(obj->u.str.str);
    }

    free(obj);
}

void free_minim_objects(MinimObject **objs, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (objs[i]) free_minim_object(objs[i]);
    free(objs);
}

bool minim_equalp(MinimObject *a, MinimObject *b)
{
    if (MINIM_OBJ_NUMBERP(a) && MINIM_OBJ_NUMBERP(b))
        return minim_number_cmp(a, b) == 0;

    if (a->type != b->type)
        return false;

    switch (a->type)
    {
    case MINIM_OBJ_VOID:
        return true;

    case MINIM_OBJ_BOOL:
        return a->u.ints.i1 == b->u.ints.i1;

    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
        return strcmp(a->u.str.str, b->u.str.str) == 0;

    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
        return a->u.ptrs.p1 == b->u.ptrs.p1;

    case MINIM_OBJ_PAIR:
        return minim_cons_eqp(a, b);
    
    case MINIM_OBJ_CLOSURE:
        return a->u.ptrs.p1 == b->u.ptrs.p1;
    
    case MINIM_OBJ_AST:
        return ast_equalp(a->u.ptrs.p1, b->u.ptrs.p1);

    case MINIM_OBJ_VECTOR:
        return minim_vector_equalp(a, b);
    
    /*
    case MINIM_OBJ_EXIT:
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
        writei_buffer(bf, (obj->u.ints.i1 ? 0 : 1));
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

    // Dump integer limbs
    case MINIM_OBJ_EXACT:
        write_buffer(bf, MINIM_EXACT(obj)->_mp_num._mp_d,
                     abs(MINIM_EXACT(obj)->_mp_num._mp_size) * sizeof(mp_limb_t));
        write_buffer(bf, MINIM_EXACT(obj)->_mp_den._mp_d,
                     abs(MINIM_EXACT(obj)->_mp_den._mp_size) * sizeof(mp_limb_t));
        break;

    case MINIM_OBJ_INEXACT:
        write_buffer(bf, &MINIM_INEXACT(obj), sizeof(double));
        break;

    case MINIM_OBJ_AST:
        ast_dump_in_buffer(obj->u.ptrs.p1, bf);
        break;

    case MINIM_OBJ_CLOSURE:
        minim_lambda_to_buffer(obj->u.ptrs.p1, bf);
        break;

    case MINIM_OBJ_VECTOR:
        minim_vector_bytes(obj, bf);
        break;

    /*
    case MINIM_OBJ_EXIT:
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

bool coerce_into_bool(MinimObject *obj)
{
    if (obj->type == MINIM_OBJ_BOOL)
    {
        return obj->u.ints.i1;
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        return MINIM_CAR(obj) || MINIM_CDR(obj);
    }
    else
    {
        return true;
    }
}

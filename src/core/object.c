#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
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
#include "tail_call.h"
#include "vector.h"

// Visible functions

static void gc_minim_object_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimObject *obj = (MinimObject*) ptr;

    switch (obj->type)
    {
    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
        mrk(gc, MINIM_STRING(obj));
        break;

    case MINIM_OBJ_EXACT:
    case MINIM_OBJ_ERR:
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_AST:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_HASH:
        mrk(gc, MINIM_DATA(obj));
        break;

    case MINIM_OBJ_VECTOR:
        mrk(gc, MINIM_VECTOR_ARR(obj));
        break;

    case MINIM_OBJ_PAIR:
        mrk(gc, MINIM_CAR(obj));
        mrk(gc, MINIM_CDR(obj));
        break;
    
    default:
        break;
    }
}

void initv_minim_object(MinimObject **pobj, MinimObjectType type, va_list vargs)
{
    MinimObject *obj;

    obj = GC_alloc_opt(sizeof(MinimObject), NULL, gc_minim_object_mrk);
    obj->type = type;

    // if (type == MINIM_OBJ_VOID)
    if (type == MINIM_OBJ_EXIT)
    {
        obj->u.ints.i1 = va_arg(vargs, long);
    }
    else if (type == MINIM_OBJ_BOOL)
    {
        obj->u.ints.i1 = va_arg(vargs, long);
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
        obj->u.str.str = GC_alloc_atomic((strlen(src) + 1) * sizeof(char));
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
    else if (MINIM_OBJ_TAIL_CALLP(obj))
    {
        MINIM_DATA(obj) = va_arg(vargs, MinimBuiltin);
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

void copy_minim_object(MinimObject **pobj, MinimObject *src)
{
    MinimObject *obj = GC_alloc_opt(sizeof(MinimObject), NULL, gc_minim_object_mrk);
    obj->type = src->type;
    *pobj = obj;

    if (src->type == MINIM_OBJ_BOOL || src->type == MINIM_OBJ_EXIT)
    {
        obj->u.ints.i1 = src->u.ints.i1;
    }
    else if (MINIM_OBJ_EXACTP(src))
    {
        mpq_ptr num = gc_alloc_mpq_ptr();
        
        mpq_set(num, MINIM_EXACT(src));
        obj->u.ptrs.p1 = num;
    }
    else if (MINIM_OBJ_INEXACTP(src))
    {
        MINIM_INEXACT(obj) = MINIM_INEXACT(src);
    }
    else if (src->type == MINIM_OBJ_SYM || src->type == MINIM_OBJ_STRING)
    {
        obj->u.str.str = GC_alloc_atomic((strlen(src->u.str.str) + 1) * sizeof(char));
        strcpy(obj->u.str.str, src->u.str.str);
    }
    else if (src->type == MINIM_OBJ_ERR)
    {
        MinimError *err;

        copy_minim_error(&err, src->u.ptrs.p1);
        obj->u.ptrs.p1 = err;
    }
    else if (src->type == MINIM_OBJ_PAIR)
    {
        if (src->u.pair.car)    copy_minim_object(&obj->u.pair.car, src->u.pair.car);
        else                    obj->u.pair.car = NULL;

        if (src->u.pair.cdr)    copy_minim_object(&obj->u.pair.cdr, src->u.pair.cdr);
        else                    obj->u.pair.cdr = NULL;
    }
    else if (src->type == MINIM_OBJ_VOID || src->type == MINIM_OBJ_FUNC ||
                src->type == MINIM_OBJ_SYNTAX)
    {
        obj->u.ptrs.p1 = src->u.ptrs.p1;   // no copy
    }
    else if (MINIM_OBJ_TAIL_CALLP(src))
    {
        MinimTailCall *call;

        copy_minim_tail_call(&call, MINIM_DATA(src));
        MINIM_DATA(obj) = call;
    }
    else if (src->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam;

        copy_minim_lambda(&lam, src->u.ptrs.p1);
        obj->u.ptrs.p1 = lam;
    }
    else if (src->type == MINIM_OBJ_AST)
    {
        SyntaxNode *node;
        copy_syntax_node(&node, src->u.ptrs.p1);
        obj->u.ptrs.p1 = node;
    }
    else if (src->type == MINIM_OBJ_SEQ)
    {
        MinimSeq *seq;
        copy_minim_seq(&seq, src->u.ptrs.p1);
        obj->u.ptrs.p1 = seq;
    }
    else if (src->type == MINIM_OBJ_HASH)
    {
        MinimHash *ht;
        copy_minim_hash_table(&ht, src->u.ptrs.p1);
        obj->u.ptrs.p1 = ht;
    }
    else if (src->type == MINIM_OBJ_VECTOR)
    {
        obj->u.vec.arr = GC_alloc(src->u.vec.len * sizeof(MinimObject*));
        obj->u.vec.len = src->u.vec.len;
        for (size_t i = 0; i < src->u.vec.len; ++i)
            copy_minim_object(&obj->u.vec.arr[i], src->u.vec.arr[i]);
    }
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
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_ERR:
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
    case MINIM_OBJ_TAIL_CALL:
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

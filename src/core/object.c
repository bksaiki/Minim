#include <stdarg.h>
#include <string.h>

#include "../gc/gc.h"
#include "env.h"
#include "error.h"
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

MinimObject *minim_void()
{
    MinimObject *o = GC_alloc(minim_void_size);
    o->type = MINIM_OBJ_VOID;
    return o;
}

MinimObject *minim_bool(int val)
{
    MinimObject *o = GC_alloc(minim_void_size);
    o->type = MINIM_OBJ_BOOL;
    MINIM_BOOL_VAL(o) = (val ? 1 : 0);
    return o;
}

MinimObject *minim_exactnum(void *num)
{
    MinimObject *o = GC_alloc(minim_exactnum_size);
    o->type = MINIM_OBJ_EXACT;
    MINIM_EXACTNUM(o) = num;
    return o;
}

MinimObject *minim_inexactnum(double num)
{
    MinimObject *o = GC_alloc(minim_inexactnum_size);
    o->type = MINIM_OBJ_INEXACT;
    MINIM_INEXACTNUM(o) = num;
    return o;
}

MinimObject *minim_symbol(char *sym)
{
    MinimObject *o = GC_alloc(minim_symbol_size);
    o->type = MINIM_OBJ_SYM;
    MINIM_SYMBOL(o) = sym;
    return o;
}

MinimObject *minim_string(char *str)
{
    MinimObject *o = GC_alloc(minim_string_size);
    o->type = MINIM_OBJ_STRING;
    MINIM_STRING(o) = str;
    return o;
}

MinimObject *minim_cons(void *car, void *cdr)
{
    MinimObject *o = GC_alloc(minim_cons_size);
    o->type = MINIM_OBJ_PAIR;
    MINIM_CAR(o) = car;
    MINIM_CDR(o) = cdr;
    return o;
}

MinimObject *minim_vector(size_t len, void *arr)
{
    MinimObject *o = GC_alloc(minim_vector_size);
    o->type = MINIM_OBJ_VECTOR;
    MINIM_VEC_ARR(o) = arr;
    MINIM_VEC_LEN(o) = len;
    return o;
}

MinimObject *minim_hash_table(void *ht)
{
    MinimObject *o = GC_alloc(minim_hash_table_size);
    o->type = MINIM_OBJ_HASH;
    MINIM_HASH_TABLE(o) = ht;
    return o;
}

MinimObject *minim_promise(void *val, void *env)
{
    MinimObject *o = GC_alloc(minim_promise_size);
    o->type = MINIM_OBJ_PROMISE;
    MINIM_PROMISE_VAL(o) = val;
    MINIM_PROMISE_ENV(o) = env;
    MINIM_PROMISE_SET_STATE(o, 0);
    return o;
}

MinimObject *minim_builtin(void *func)
{
    MinimObject *o = GC_alloc(minim_builtin_size);
    o->type = MINIM_OBJ_FUNC;
    MINIM_BUILTIN(o) = func;
    return o;
}

MinimObject *minim_syntax(void *func)
{
    MinimObject *o = GC_alloc(minim_syntax_size);
    o->type = MINIM_OBJ_SYNTAX;
    MINIM_SYNTAX(o) = func;
    return o;
}

MinimObject *minim_closure(void *closure)
{
    MinimObject *o = GC_alloc(minim_closure_size);
    o->type = MINIM_OBJ_CLOSURE;
    MINIM_CLOSURE(o) = closure;
    return o;
}

MinimObject *minim_tail_call(void *tc)
{
    MinimObject *o = GC_alloc(minim_tail_call_size);
    o->type = MINIM_OBJ_TAIL_CALL;
    MINIM_TAIL_CALL(o) = tc;
    return o;
}

MinimObject *minim_transform(char *name, void *closure)
{
    MinimObject *o = GC_alloc(minim_transform_size);
    o->type = MINIM_OBJ_TRANSFORM;
    MINIM_TRANSFORM_NAME(o) = name;
    MINIM_TRANSFORM_PROC(o) = name;
    return o;
}

MinimObject *minim_ast(void *ast)
{
    MinimObject *o = GC_alloc(minim_ast_size);
    o->type = MINIM_OBJ_AST;
    MINIM_AST(o) = ast;
    return o;
}

MinimObject *minim_sequence(void *seq)
{
    MinimObject *o = GC_alloc(minim_sequence_size);
    o->type = MINIM_OBJ_SEQ;
    MINIM_SEQUENCE(o) = seq;
    return o;
}

MinimObject *minim_values(size_t len, void *arr)
{
    MinimObject *o = GC_alloc(minim_values_size);
    o->type = MINIM_OBJ_VALUES;
    MINIM_VALUES(o) = arr;
    MINIM_VALUES_SIZE(o) = len;
    return o;
}

MinimObject *minim_error(void *err)
{
    MinimObject *o = GC_alloc(minim_error_size);
    o->type = MINIM_OBJ_ERR;
    MINIM_ERROR(o) = err;
    return o;
}

MinimObject *minim_exit(long code)
{
    MinimObject *o = GC_alloc(minim_exit_size);
    o->type = MINIM_OBJ_EXIT;
    MINIM_EXIT_VAL(o) = code;
    return o;
}

bool minim_equalp(MinimObject *a, MinimObject *b)
{
    if (a == b)                 return true;        // early exit, same object
    if (a->type != b->type)     return false;       // early exit, different object

    switch (a->type)
    {
    case MINIM_OBJ_EXACT:
    case MINIM_OBJ_INEXACT:
        return minim_number_cmp(a, b) == 0;

    case MINIM_OBJ_BOOL:
        return MINIM_BOOL_VAL(a) == MINIM_BOOL_VAL(b);
    
    case MINIM_OBJ_VOID:
    case MINIM_OBJ_ERR:
        return true;

    /*
    case MINIM_OBJ_EXIT:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
    case MINIM_OBJ_ERR:
    case MINIM_OBJ_VALUES:
    */
    default:
        return false;
    }

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

    case MINIM_OBJ_PROMISE:
        return minim_equalp(MINIM_CAR(a), MINIM_CAR(b)) && 
               MINIM_PROMISE_FORCEDP(a) == MINIM_PROMISE_FORCEDP(b);
    
    /*
    case MINIM_OBJ_EXIT:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
    case MINIM_OBJ_ERR:
    case MINIM_OBJ_VALUES:
    */
    default:
        return false;
    }
}

Buffer* minim_obj_to_bytes(MinimObject *obj)
{
    Buffer *bf, *bf2;
    
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

    case MINIM_OBJ_PROMISE:
        bf2 = minim_obj_to_bytes(MINIM_CAR(obj));
        writeb_buffer(bf, bf2);
        break;

    /*
    case MINIM_OBJ_EXIT:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
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

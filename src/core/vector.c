#include <stdlib.h>
#include "assert.h"
#include "builtin.h"
#include "error.h"
#include "list.h"
#include "number.h"
#include "vector.h"

bool minim_vector_equalp(MinimObject *a, MinimObject *b)
{
    if (a->u.vec.len != b->u.vec.len)
        return false;

    for (size_t i = 0; i < a->u.vec.len; ++i)
    {
        if (!minim_equalp(a->u.vec.arr[i], b->u.vec.arr[i]))
            return false;
    }

    return true;
}

void minim_vector_bytes(MinimObject *v, Buffer *bf)
{
    Buffer *in;

    for (size_t i = 0; i < v->u.vec.len; ++i)
    {
        in = minim_obj_to_bytes(v->u.vec.arr[i]);
        writeb_buffer(bf, in);
        free_buffer(in);
    } 
}

//
//  Builtins
//

MinimObject *minim_builtin_make_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimObject **arr;
    size_t size;
    
    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("exact non-negative integer", "make-vector", 0, args[0]);
    
    size = minim_number_to_uint(args[0]);
    arr = malloc(size * sizeof(MinimObject*));
    for (size_t i = 0; i < size; ++i)
    {
        MinimNumber *zero;
        init_minim_number(&zero, MINIM_NUMBER_EXACT);
        str_to_minim_number(zero, "0");
        init_minim_object(&arr[i], MINIM_OBJ_NUM, zero);
    }   

    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, size);
    return res;
}

MinimObject *minim_builtin_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimObject **arr;

    arr = malloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        arr[i] = copy2_minim_object(args[i]);

    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, argc);
    return res;
}

MinimObject *minim_builtin_vector_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-ref", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "vector-ref", 1, args[1]);

    idx = minim_number_to_uint(args[1]);
    if  (idx >= args[0]->u.vec.len)
    {
        minim_error(&res, "Index out of bounds: %lu", idx);
        return res;
    }
    
    return copy2_minim_object(args[0]->u.vec.arr[idx]);
}

MinimObject *minim_builtin_vector_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-set!", 0, args[0]);

    if (MINIM_OBJ_OWNERP(args[0]))
        return minim_argument_error("reference to a vector", "vector-set!", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "vector-set!", 1, args[1]);

    idx = minim_number_to_uint(args[1]);
    if  (idx >= args[0]->u.vec.len)
    {
        minim_error(&res, "Index out of bounds: %lu", idx);
        return res;
    }

    free_minim_object(args[0]->u.vec.arr[idx]);
    args[0]->u.vec.arr[idx] = copy2_minim_object(args[2]);
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_vector_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector->list", 0, args[0]);

    if (args[0]->u.vec.len > 0)
    {
        MinimObject *it, *cp;
        bool first = true;

        for (size_t i = 0; i < args[0]->u.vec.len; ++i)
        {
            copy_minim_object(&cp, args[0]->u.vec.arr[i]);
            if (first)
            {
                init_minim_object(&res, MINIM_OBJ_PAIR, cp, NULL);
                it = res;
                first = false;
            }
            else
            {
                init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, cp, NULL);
                it = MINIM_CDR(it);
            }
        }
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    }

    return res;
}

MinimObject *minim_builtin_list_to_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;
    MinimObject **arr;
    size_t len;

    if (!minim_listp(args[0]))
        return minim_argument_error("list", "list->vector", 0, args[0]);

    len = minim_list_length(args[0]);
    arr = malloc(len * sizeof(MinimObject*));
    it = args[0];
    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
        arr[i] = copy2_minim_object(MINIM_CAR(it));

    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, len);
    return res;
}
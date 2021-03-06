#include "../gc/gc.h"
#include "assert.h"
#include "builtin.h"
#include "error.h"
#include "list.h"
#include "number.h"
#include "vector.h"

bool minim_vector_equalp(MinimObject *a, MinimObject *b)
{
    if (MINIM_VECTOR_LEN(a) != MINIM_VECTOR_LEN(b))
        return false;

    for (size_t i = 0; i < MINIM_VECTOR_LEN(a); ++i)
    {
        if (!minim_equalp(MINIM_VECTOR_ARR(a)[i], MINIM_VECTOR_ARR(b)[i]))
            return false;
    }

    return true;
}

void minim_vector_bytes(MinimObject *v, Buffer *bf)
{
    Buffer *in;

    for (size_t i = 0; i < MINIM_VECTOR_LEN(v); ++i)
    {
        in = minim_obj_to_bytes(MINIM_VECTOR_ARR(v)[i]);
        writeb_buffer(bf, in);
    } 
}

//
//  Builtins
//

MinimObject *minim_builtin_make_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *obj;
    MinimObject **arr;
    size_t size;
    
    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("exact non-negative integer", "make-vector", 0, args[0]);
    
    size = MINIM_NUMBER_TO_UINT(args[0]);
    arr = GC_alloc(size * sizeof(MinimObject*));

    obj = (argc == 2) ? args[1] : int_to_minim_number(0);
    for (size_t i = 0; i < size; ++i)
        arr[i] = obj;
    
    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, size);
    return res;
}

MinimObject *minim_builtin_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimObject **arr;

    arr = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        arr[i] = args[i];

    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, argc);
    return res;
}

MinimObject *minim_builtin_vector_length(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-length", 0, args[0]);

    return uint_to_minim_number(MINIM_VECTOR_LEN(args[0]));
    return res;
}

MinimObject *minim_builtin_vector_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-ref", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "vector-ref", 1, args[1]);

    idx = MINIM_NUMBER_TO_UINT(args[1]);
    if  (idx >= MINIM_VECTOR_LEN(args[0]))
        return minim_error("index out of bounds: ~u", "vector-ref", idx);
    
    return MINIM_VECTOR_ARR(args[0])[idx];
}

MinimObject *minim_builtin_vector_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-set!", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "vector-set!", 1, args[1]);

    idx = MINIM_NUMBER_TO_UINT(args[1]);
    if  (idx >= MINIM_VECTOR_LEN(args[0]))
        return minim_error("index out of bounds: ~u", "vector-set!", idx);

    MINIM_VECTOR_ARR(args[0])[idx] = args[2];
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_vector_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector->list", 0, args[0]);

    if (MINIM_VECTOR_LEN(args[0]) > 0)
    {
        MinimObject *it, *cp;
        bool first = true;

        for (size_t i = 0; i < MINIM_VECTOR_LEN(args[0]); ++i)
        {
            cp = MINIM_VECTOR_ARR(args[0])[i];
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
    arr = GC_alloc(len * sizeof(MinimObject*));
    it = args[0];
    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
        arr[i] = MINIM_CAR(it);

    init_minim_object(&res, MINIM_OBJ_VECTOR, arr, len);
    return res;
}

MinimObject *minim_builtin_vector_fillb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_VECTORP(args[0]))
        return minim_argument_error("vector", "vector-fill!", 0, args[0]);

    for (size_t i = 0; i < MINIM_VECTOR_LEN(args[0]); ++i)
        MINIM_VECTOR_ARR(args[0])[i] = args[1];
    
    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;   
}

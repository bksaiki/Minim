#include <stdlib.h>
#include "assert.h"
#include "builtin.h"
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

bool assert_vector(MinimObject *obj, MinimObject **err, const char *msg)
{
    if (!minim_vectorp(obj))
    {
        minim_error(err, msg);
        return false;
    }

    return true;
}

bool minim_vectorp(MinimObject *obj)
{
    return obj->type == MINIM_OBJ_VECTOR;
}

//
//  Builtins
//

MinimObject *minim_builtin_make_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "make-vector", 1, argc) &&
        assert_exact_nonneg_int(args[0], &res, "Expected a non-negative size in the first argument of 'make-vector'"))
    {
        MinimObject **arr;
        size_t size = minim_number_to_uint(args[0]);

        arr = malloc(size * sizeof(MinimObject*));
        for (size_t i = 0; i < size; ++i)
        {
            MinimNumber *zero;
            init_minim_number(&zero, MINIM_NUMBER_EXACT);
            str_to_minim_number(zero, "0");
            init_minim_object(&arr[i], MINIM_OBJ_NUM, zero);
        }

        init_minim_object(&res, MINIM_OBJ_VECTOR, arr, size);
    }

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

    if (assert_exact_argc(&res, "vector-ref", 2, argc) &&
        assert_vector(args[0], &res, "Expected a vector in the first argument of 'vector-ref") &&
        assert_exact_nonneg_int(args[1], &res, "Expected a non-negative index in the second argument of 'vector-ref'"))
    { 
        size_t idx = minim_number_to_uint(args[1]);

        if (assert_generic(&res, "Index out of bounds", idx < args[0]->u.vec.len))
            res = copy2_minim_object(args[0]->u.vec.arr[idx]);
    }

    return res;
}

MinimObject *minim_builtin_vector_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "vector-set!", 3, argc) &&
        assert_vector(args[0], &res, "Expected a vector in the first argument of 'vector-set!") &&
        assert_generic(&res, "Expected a reference to a existing vector", !MINIM_OBJ_OWNERP(args[0])) &&
        assert_exact_nonneg_int(args[1], &res, "Expected a non-negative index in the second argument of 'vector-set!'"))
    {
        size_t idx = minim_number_to_uint(args[1]);

        if (assert_generic(&res, "Index out of bounds", idx < args[0]->u.vec.len))
        {   
            free_minim_object(args[0]->u.vec.arr[idx]);
            args[0]->u.vec.arr[idx] = copy2_minim_object(args[2]);
            init_minim_object(&res, MINIM_OBJ_VOID);
        }
    }

    return res;
}

MinimObject *minim_builtin_vector_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "vector->list", 1, argc) &&
        assert_vector(args[0], &res, "Expected a vector in the first argument of 'vector->list"))
    {
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
    }

    return res;
}

MinimObject *minim_builtin_list_to_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "list->vector", 1, argc) &&
        assert_list(args[0], &res, "Expected a list in the second argument of list->vector"))
    {
        MinimObject **arr;
        MinimObject *it;
        size_t len;
        
        len = minim_list_length(args[0]);
        arr = malloc(len * sizeof(MinimObject*));
        it = args[0];

        for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
            arr[i] = copy2_minim_object(MINIM_CAR(it));
        init_minim_object(&res, MINIM_OBJ_VECTOR, arr, len);
    }

    return res;
}
#include <stdlib.h>
#include "assert.h"
#include "builtin.h"
#include "list.h"
#include "number.h"
#include "vector.h"

void init_minim_vector(MinimVector **pvec, size_t size)
{
    MinimVector *vec = malloc(sizeof(MinimVector));
    vec->arr = calloc(size, sizeof(MinimObject*));
    vec->size = size;
    *pvec = vec;
}

void copy_minim_vector(MinimVector **pvec, MinimVector *src)
{
    MinimVector *vec = malloc(sizeof(MinimVector));
    vec->arr = malloc(src->size * sizeof(MinimObject*));
    vec->size = src->size;
    *pvec = vec;

    for (size_t i = 0; i < src->size; ++i)
        copy_minim_object(&vec->arr[i], src->arr[i]);
}

void free_minim_vector(MinimVector *vec)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        if (vec->arr[i])
            free_minim_object(vec->arr[i]);
    }

    free(vec->arr);
    free(vec);
}

bool minim_vector_equalp(MinimVector *a, MinimVector *b)
{
    if (a->size != b->size)
        return false;

    for (size_t i = 0; i < a->size; ++i)
    {
        if (!minim_equalp(a->arr[i], b->arr[i]))
            return false;
    }

    return true;
}

void minim_vector_bytes(MinimVector *vec, Buffer *bf)
{
    Buffer *in;

    write_buffer(bf, &vec->size, sizeof(size_t));
    for (size_t i = 0; i < vec->size; ++i)
    {
        in = minim_obj_to_bytes(vec->arr[i]);
        writeb_buffer(bf, in);
        free_buffer(in);
    } 
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
        MinimNumber *num;
        MinimVector *vec;
        size_t size;

        num = args[0]->data;
        size = mpz_get_ui(mpq_numref(num->rat));
        init_minim_vector(&vec, size);

        for (size_t i = 0; i < size; ++i)
        {
            MinimNumber *zero;
            init_minim_number(&zero, MINIM_NUMBER_EXACT);
            str_to_minim_number(zero, "0");
            init_minim_object(&vec->arr[i], MINIM_OBJ_NUM, zero);
        }

        init_minim_object(&res, MINIM_OBJ_VECTOR, vec);
    }

    return res;
}

MinimObject *minim_builtin_vector(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimVector *vec;

    init_minim_vector(&vec, argc);
    for (size_t i = 0; i < argc; ++i)
        vec->arr[i] = copy2_minim_object(args[i]);

    init_minim_object(&res, MINIM_OBJ_VECTOR, vec);
    return res;
}

MinimObject *minim_builtin_vector_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "vector-ref", 2, argc) &&
        assert_generic(&res, "Expected a vector in the first argument of 'vector-ref", minim_vectorp(args[0])) &&
        assert_exact_nonneg_int(args[1], &res, "Expected a non-negative index in the second argument of 'vector-ref'"))
    {
        MinimVector *vec = args[0]->data;
        MinimNumber *num = args[1]->data;
        size_t idx = mpz_get_ui(mpq_numref(num->rat));

        if (assert_generic(&res, "Index out of bounds", idx < vec->size))
            res = copy2_minim_object(vec->arr[idx]);
    }

    return res;
}
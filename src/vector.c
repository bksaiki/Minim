#include <stdlib.h>
#include "assert.h"
#include "builtin.h"
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
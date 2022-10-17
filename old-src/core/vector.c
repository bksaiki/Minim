#include "minimpriv.h"

void minim_vector_bytes(MinimObject *v, Buffer *bf)
{
    Buffer *in;

    for (size_t i = 0; i < MINIM_VECTOR_LEN(v); ++i)
    {
        in = minim_obj_to_bytes(MINIM_VECTOR_REF(v, i));
        writeb_buffer(bf, in);
    } 
}

//
//  Builtins
//

MinimObject *minim_builtin_vector(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *v = minim_vector(argc);
    for (size_t i = 0; i < argc; ++i)
        MINIM_VECTOR_REF(v, i) = args[i];
    return v;
}

MinimObject *minim_builtin_make_vector(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *init;
    size_t size;
    
    if (!minim_exact_nonneg_intp(args[0]))
        THROW(env, minim_argument_error("exact non-negative integer", "make-vector", 0, args[0]));
    
    size = MINIM_NUMBER_TO_UINT(args[0]);
    init = (argc == 2) ? args[1] : int_to_minim_number(0);
    return minim_vector2(size, init);
}

MinimObject *minim_builtin_vectorp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_VECTORP(args[0]));
}

MinimObject *minim_builtin_vector_length(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_VECTORP(args[0]))
        THROW(env, minim_argument_error("vector", "vector-length", 0, args[0]));

    return uint_to_minim_number(MINIM_VECTOR_LEN(args[0]));
}

MinimObject *minim_builtin_vector_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        THROW(env, minim_argument_error("vector", "vector-ref", 0, args[0]));

    if (!minim_exact_nonneg_intp(args[1]))
        THROW(env, minim_argument_error("exact non-negative integer", "vector-ref", 1, args[1]));

    idx = MINIM_NUMBER_TO_UINT(args[1]);
    if  (idx >= MINIM_VECTOR_LEN(args[0]))
        THROW(env, minim_error("index out of bounds: ~u", "vector-ref", idx));
    
    return MINIM_VECTOR_REF(args[0], idx);
}

MinimObject *minim_builtin_vector_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t idx;

    if (!MINIM_OBJ_VECTORP(args[0]))
        THROW(env, minim_argument_error("vector", "vector-set!", 0, args[0]));

    if (!minim_exact_nonneg_intp(args[1]))
        THROW(env, minim_argument_error("exact non-negative integer", "vector-set!", 1, args[1]));

    idx = MINIM_NUMBER_TO_UINT(args[1]);
    if  (idx >= MINIM_VECTOR_LEN(args[0]))
        THROW(env, minim_error("index out of bounds: ~u", "vector-set!", idx));

    MINIM_VECTOR_REF(args[0], idx) = args[2];
    return minim_void;
}

MinimObject *minim_builtin_vector_to_list(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_VECTORP(args[0]))
        THROW(env, minim_argument_error("vector", "vector->list", 0, args[0]));

    return minim_list(MINIM_VECTOR_ARR(args[0]), MINIM_VECTOR_LEN(args[0]));
}

MinimObject *minim_builtin_list_to_vector(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *it, *v;
    size_t len;

    if (!minim_listp(args[0]))
        THROW(env, minim_argument_error("list", "list->vector", 0, args[0]));

    len = minim_list_length(args[0]);
    v = minim_vector(len);
    it = args[0];

    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
        MINIM_VECTOR_REF(v, i) = MINIM_CAR(it);
    return v;
}

MinimObject *minim_builtin_vector_fillb(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_VECTORP(args[0]))
        THROW(env, minim_argument_error("vector", "vector-fill!", 0, args[0]));

    for (size_t i = 0; i < MINIM_VECTOR_LEN(args[0]); ++i)
        MINIM_VECTOR_REF(args[0], i) = args[1];
    
    return minim_void;
}

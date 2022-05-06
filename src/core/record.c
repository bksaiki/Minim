#include "minimpriv.h"

MinimObject *minim_builtin_make_record(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *init;
    size_t nfields;

    if (!minim_exact_nonneg_intp(args[0]))
        THROW(env, minim_argument_error("exact non-negative integer", "make-vector", 0, args[0]));

    nfields = MINIM_NUMBER_TO_UINT(args[0]);
    init = ((argc == 2) ? args[1] : minim_false);
    return minim_record(nfields, init);
}

MinimObject *minim_builtin_record(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *rec;
    
    rec = minim_record(argc - 1, NULL);
    MINIM_RECORD_TYPE(rec) = args[0];
    for (size_t i = 1; i < argc; ++i)
        MINIM_RECORD_REF(rec, i - 1) = args[i];

    return rec;
}

MinimObject *minim_builtin_recordp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_RECORDP(args[0]));
}

MinimObject *minim_builtin_record_length(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_RECORDP(args[0]))
        THROW(env, minim_argument_error("record", "record-length", 0, args[0])); 
    return uint_to_minim_number(MINIM_RECORD_NFIELDS(args[0]));
}

MinimObject *minim_builtin_record_type(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_RECORDP(args[0]))
        THROW(env, minim_argument_error("record", "record-type", 0, args[0]));
    return MINIM_RECORD_TYPE(args[0]);
}

MinimObject *minim_builtin_record_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t index;

    if (!MINIM_OBJ_RECORDP(args[0]))
        THROW(env, minim_argument_error("record", "record-ref", 0, args[0]));

    if (!minim_exact_nonneg_intp(args[1]))
        THROW(env, minim_argument_error("exact non-negative integer", "record-ref", 1, args[1]));

    index = MINIM_NUMBER_TO_UINT(args[1]);
    return MINIM_RECORD_REF(args[0], index);
}

MinimObject *minim_builtin_record_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t index;

    if (!MINIM_OBJ_RECORDP(args[0]))
        THROW(env, minim_argument_error("record", "record-set!", 0, args[0]));

    if (!minim_exact_nonneg_intp(args[1]))
        THROW(env, minim_argument_error("exact non-negative integer", "record-set!", 1, args[1]));

    index = MINIM_NUMBER_TO_UINT(args[1]);
    MINIM_RECORD_REF(args[0], index) = args[2];
    return minim_void;
}

MinimObject *minim_builtin_record_set_typeb(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_RECORDP(args[0]))
        THROW(env, minim_argument_error("record", "record-set!", 0, args[0]));

    MINIM_RECORD_TYPE(args[0]) = args[1];
    return minim_void;
}

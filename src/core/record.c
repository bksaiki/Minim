#include "minimpriv.h"

MinimObject *minim_builtin_make_record(MinimEnv *env, size_t argc, MinimObject **args)
{
    
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
    
}

MinimObject *minim_builtin_record_length(MinimEnv *env, size_t argc, MinimObject **args)
{
    
}

MinimObject *minim_builtin_record_type(MinimEnv *env, size_t argc, MinimObject **args)
{
    

}

MinimObject *minim_builtin_record_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    
}

MinimObject *minim_builtin_record_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    
}

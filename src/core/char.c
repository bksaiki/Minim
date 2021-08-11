#include "builtin.h"
#include "error.h"
#include "number.h"

MinimObject *minim_builtin_charp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_CHARP(args[0]));
}

MinimObject *minim_builtin_int_to_char(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *limit;
    
    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("valid character integer value", "integer->char", 0, args[0]);

    limit = uint_to_minim_number(256);
    if (minim_number_cmp(args[0], limit) >= 0)
        return minim_argument_error("valid character integer value", "integer->char", 0, args[0]);

    return minim_char(MINIM_NUMBER_TO_UINT(args[0]));
}

MinimObject *minim_builtin_char_to_int(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character?", "char->integer", 0, args[0]);

    return uint_to_minim_number(MINIM_CHAR(args[0]));
}

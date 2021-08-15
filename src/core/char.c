#include <ctype.h>

#include "builtin.h"
#include "error.h"
#include "number.h"

MinimObject *minim_builtin_charp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_CHARP(args[0]));
}

MinimObject *minim_builtin_char_eqp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char=?", 1, args[1]);

    return to_bool(MINIM_CHAR(args[0]) == MINIM_CHAR(args[1]));
}

MinimObject *minim_builtin_char_gtp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char>?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char>?", 1, args[1]);

    return to_bool(MINIM_CHAR(args[0]) > MINIM_CHAR(args[1]));
}

MinimObject *minim_builtin_char_ltp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char<?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char<?", 1, args[1]);

    return to_bool(MINIM_CHAR(args[0]) < MINIM_CHAR(args[1]));
}

MinimObject *minim_builtin_char_gtep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char>=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char>=?", 1, args[1]);

    return to_bool(MINIM_CHAR(args[0]) >= MINIM_CHAR(args[1]));
}

MinimObject *minim_builtin_char_ltep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char<=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char<=?", 1, args[1]);

    return to_bool(MINIM_CHAR(args[0]) <= MINIM_CHAR(args[1]));
}

MinimObject *minim_builtin_char_ci_eqp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-ci=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char-ci=?", 1, args[1]);

    return to_bool(toupper(MINIM_CHAR(args[0])) == toupper(MINIM_CHAR(args[1])));
}

MinimObject *minim_builtin_char_ci_gtp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-ci>?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char-ci>?", 1, args[1]);

    return to_bool(toupper(MINIM_CHAR(args[0])) > toupper(MINIM_CHAR(args[1])));
}

MinimObject *minim_builtin_char_ci_ltp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-ci<?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char-ci<?", 1, args[1]);

    return to_bool(toupper(MINIM_CHAR(args[0])) < toupper(MINIM_CHAR(args[1])));
}

MinimObject *minim_builtin_char_ci_gtep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-ci>=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char-ci>=?", 1, args[1]);

    return to_bool(toupper(MINIM_CHAR(args[0])) >= toupper(MINIM_CHAR(args[1])));
}

MinimObject *minim_builtin_char_ci_ltep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-ci<=?", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "char-ci<=?", 1, args[1]);

    return to_bool(toupper(MINIM_CHAR(args[0])) <= toupper(MINIM_CHAR(args[1])));
}

MinimObject *minim_builtin_char_alphabeticp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-alphabetic?", 0, args[0]);

    return to_bool(isalpha(MINIM_CHAR(args[0])));
}

MinimObject *minim_builtin_char_numericp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-numeric?", 0, args[0]);

    return to_bool(isdigit(MINIM_CHAR(args[0])));
}

MinimObject *minim_builtin_char_whitespacep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-whitespace?", 0, args[0]);

    return to_bool(isspace(MINIM_CHAR(args[0])));
}

MinimObject *minim_builtin_char_upper_casep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-upper-case?", 0, args[0]);

    return to_bool(isupper(MINIM_CHAR(args[0])));
}

MinimObject *minim_builtin_char_lower_casep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-lower-case?", 0, args[0]);

    return to_bool(islower(MINIM_CHAR(args[0])));
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

MinimObject *minim_builtin_char_upcase(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-upcase", 0, args[0]);

    return minim_char(toupper(MINIM_CHAR(args[0])));
}

MinimObject *minim_builtin_char_downcase(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_CHARP(args[0]))
        return minim_argument_error("character", "char-downcase", 0, args[0]);

    return minim_char(tolower(MINIM_CHAR(args[0])));
}

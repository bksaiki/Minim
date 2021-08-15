#include <limits.h>
#include <string.h>

#include "../common/buffer.h"
#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "number.h"

void replace_special_chars(char *str)
{
    size_t src = 0, dest = 0;

    while (str[src])
    {
        if (str[src] == '\\')
        {
            ++src;
            if (str[src] == 'n')        str[dest] = '\n';
            else if (str[src] == '"')   str[dest] = '"';
            else                        printf("Unknown escape character: \\%c", str[src]);
        }
        else if (dest != src)
        {
            str[dest] = str[src];
        }

        ++src;
        ++dest;
    }

    str[dest] = '\0';
}

// *** Builtins *** //

MinimObject *minim_builtin_stringp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_STRINGP(args[0]));
}

MinimObject *minim_builtin_make_string(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *str;
    size_t len;
    char fill;

    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("exact non-negative integer", "make-string", 0, args[0]);
    
    if (argc == 1)
    {
        fill = '?';
    }
    else
    {
        if (!MINIM_OBJ_CHARP(args[1]))
            return minim_argument_error("character", "make-string", 1, args[1]);
        fill = MINIM_CHAR(args[1]);
    }

    len = MINIM_NUMBER_TO_UINT(args[0]);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (size_t i = 0; i < len; ++i)
        str[i] = fill;

    str[len] = '\0';
    return minim_string(str);
}

MinimObject *minim_builtin_string(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *str;

    for (size_t i = 0; i < argc; ++i)
    {
        if (!MINIM_OBJ_CHARP(args[i]))
            return minim_argument_error("character", "string", i, args[i]);
    }

    str = GC_alloc_atomic((argc + 1) * sizeof(char));
    for (size_t i = 0; i < argc; ++i)
        str[i] = MINIM_CHAR(args[i]);
    
    str[argc] = '\0';
    return minim_string(str);
}

MinimObject *minim_builtin_string_length(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string-length", 0, args[0]);

    return uint_to_minim_number(strlen(MINIM_STRING(args[0])));
}

MinimObject *minim_builtin_string_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t len, idx;
    char *str;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string-ref", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "string-ref", 1, args[1]);

    str = MINIM_STRING(args[0]);
    len = strlen(str);
    idx = MINIM_NUMBER_TO_UINT(args[1]);

    if (idx >= len)
        return minim_argument_error("out of bounds", "string-ref", 1, args[1]);

    return minim_char(str[idx]);
}

MinimObject *minim_builtin_string_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t len, idx;
    char *str;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string-set!", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "string-set!", 1, args[1]);

    if (!MINIM_OBJ_CHARP(args[2]))
        return minim_argument_error("character", "string-set!", 2, args[2]);

    str = MINIM_STRING(args[0]);
    len = strlen(str);
    idx = MINIM_NUMBER_TO_UINT(args[1]);

    if (idx >= len)
        return minim_argument_error("out of bounds", "string-ref", 1, args[1]);

    str[idx] = MINIM_CHAR(args[2]);
    return minim_void;
}

MinimObject *minim_builtin_string_copy(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *str;
    size_t len;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string-copy", 0, args[0]);

    len = strlen(MINIM_STRING(args[0]));
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    strcpy(str, MINIM_STRING(args[0]));
    return minim_string(str);
}

MinimObject *minim_builtin_string_fillb(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *str;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string-fill!", 0, args[0]);

    if (!MINIM_OBJ_CHARP(args[1]))
        return minim_argument_error("character", "string-fill!", 1, args[1]);

    str = MINIM_STRING(args[0]);
    for (size_t i = 0; i < strlen(str); ++i)
        str[i] = MINIM_CHAR(args[1]);
    return minim_void;
}

MinimObject *minim_builtin_substring(MinimEnv *env, size_t argc, MinimObject **args)
{
    size_t len, start, end;
    char *str, *tmp;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "substring", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative integer", "substring", 1, args[1]);

    str = MINIM_STRING(args[0]);
    len = strlen(str);
    start = MINIM_NUMBER_TO_UINT(args[1]);
    if (argc == 2)
    {
        end = len;
    }
    else
    {
        if (!minim_exact_nonneg_intp(args[2]))
            return minim_argument_error("exact non-negative integer", "substring", 2, args[2]);
        
        end = MINIM_NUMBER_TO_UINT(args[2]);
        if (start >= end || end > len)
            THROW(minim_error("expected [begin, end)", "substring"));
    }

    tmp = GC_alloc_atomic((end - start + 1) * sizeof(char));
    strncpy(tmp, &str[start], end - start);
    tmp[end - start] = '\0';

    return minim_string(tmp);
}

MinimObject *minim_builtin_string_to_symbol(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "string->symbol", 0, args[0]);

    return minim_symbol(MINIM_STRING(args[0]));
}

MinimObject *minim_builtin_symbol_to_string(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *dest;

    if (!MINIM_OBJ_SYMBOLP(args[0]))
        return minim_argument_error("symbol", "symbol->string", 0, args[0]);

    dest = GC_alloc_atomic((strlen(MINIM_STRING(args[0])) + 1) * sizeof(char));
    strcpy(dest, MINIM_STRING(args[0]));
    return minim_string(dest);
}

static size_t collect_format_vars(const char *str, size_t len)
{
    size_t cnt = 0;

    for (size_t i = 0; i < len; ++i)
    {
        if (str[i] == '~' && str[i + 1] == 'a')
            ++cnt;
    }

    return cnt;
}

MinimObject *minim_builtin_format(MinimEnv *env, size_t argc, MinimObject **args)
{
    Buffer *bf;
    size_t len, vcount, var;
    char *str;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "format", 0, args[0]);

    str = MINIM_STRING(args[0]);
    len = strlen(str);
    vcount = collect_format_vars(str, len);

    if (vcount != argc - 1)
        THROW(minim_error("number of format variables do not match argument count", "format"));

    var = 1;
    init_buffer(&bf);
    for (size_t i = 0; i < len; ++i)
    {
        if (str[i] == '~')
        {
            if (++i == len)
                THROW(minim_error("expected a character after '~'", "format"));

            if (str[i] == 'a')
            {
                PrintParams pp;
                char *t;

                set_default_print_params(&pp);
                pp.quote = true;
                pp.display = true;
                
                t = print_to_string(args[var++], env, &pp);
                writes_buffer(bf, t);
            }
            else
            {
                writec_buffer(bf, str[i]);
            }
        }
        else
        {
            writec_buffer(bf, str[i]);
        }
    }

    trim_buffer(bf);
    return minim_string(get_buffer(bf));
}

MinimObject *minim_builtin_printf(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;
    PrintParams pp;

    if (!MINIM_OBJ_STRINGP(args[0]))
        return minim_argument_error("string", "printf", 0, args[0]);

    val = minim_builtin_format(env, argc, args);
    set_default_print_params(&pp);
    pp.quote = true;
    pp.display = true;
    
    replace_special_chars(MINIM_STRING(val));
    print_minim_object(val, env, &pp);
    return minim_void;
}

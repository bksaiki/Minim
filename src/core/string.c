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

static bool assert_string_args(size_t argc, MinimObject **args, MinimObject **res, const char *name)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (!MINIM_OBJ_STRINGP(args[i]))
        {
            *res = minim_argument_error("string", name, i, args[i]);
            return false;
        }
    }
    
    return true;
}

// *** Builtins *** //

MinimObject *minim_builtin_stringp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_bool(MINIM_OBJ_STRINGP(args[0]));
}

MinimObject *minim_builtin_string_append(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    Buffer *bf;

    if (!assert_string_args(argc, args, &res, "string-append"))
        return res;

    init_buffer(&bf);
    for (size_t i = 0; i < argc; ++i)
        writes_buffer(bf, MINIM_STRING(args[i]));

    trim_buffer(bf);
    return minim_string(get_buffer(bf));
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
            return minim_error("expected [begin, end)", "substring");
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

    return minim_string(MINIM_STRING(args[0]));
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
        return minim_error("number of format variables do not match argument count", "format");

    var = 1;
    init_buffer(&bf);
    for (size_t i = 0; i < len; ++i)
    {
        if (str[i] == '~')
        {
            if (++i == len)
                return minim_error("expected a character after '~'", "format");

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
    if (MINIM_OBJ_THROWNP(val))
        return val;
    
    set_default_print_params(&pp);
    pp.quote = true;
    pp.display = true;
    
    replace_special_chars(MINIM_STRING(val));
    print_minim_object(val, env, &pp);
    return minim_void();
}

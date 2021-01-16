#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../common/buffer.h"
#include "assert.h"
#include "number.h"
#include "string.h"
#include "variable.h"

bool minim_stringp(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_STRING;
}

bool assert_string(MinimObject *thing, MinimObject **res, const char *msg)
{
    if (!assert_generic(res, msg, minim_stringp(thing)))
        return false;

    return true;
}

void replace_special_chars(char *str)
{
    size_t src = 0, dest = 0;

    while (str[src])
    {
        if (str[src] == '\\')
        {
            ++src;
            if (str[src] == 'n')        str[dest] = '\n';
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

MinimObject *minim_builtin_stringp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "string?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_stringp(args[0]));
        
    return res;
}

MinimObject *minim_builtin_string_append(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "string-append", 1, argc) &&
        assert_for_all(&res, args, argc, "Expected all string arguments for 'string-append'", minim_stringp))
    {
        Buffer *bf;

        init_buffer(&bf);
        for (size_t i = 0; i < argc; ++i)
            writes_buffer(bf, args[i]->data);

        trim_buffer(bf);
        init_minim_object(&res, MINIM_OBJ_STRING, release_buffer(bf));
        free_buffer(bf);
    }

    return res;
}

MinimObject *minim_builtin_substring(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_range_argc(&res, "substring", 2, 3, argc) &&
        assert_string(args[0], &res, "Expected a string in the 1st argument of 'substring'") &&
        assert_exact_nonneg_int(args[1], &res, "Expected a non-negative exact integer in the \
                                                2nd argument of 'substring'"))
    {
        size_t len, start, end;
        char *str, *tmp;

        str = args[0]->data;
        len = strlen(str);
        start = mpz_get_ui(mpq_numref(((MinimNumber*) args[1]->data)->rat));
        if (argc == 2)
        {
            end = len;
        }
        else
        {
            if (!assert_exact_nonneg_int(args[2], &res, "Expected a non-negative exact integer \
                                                         in the 3rd argument of 'substring'"))
                return res;
            
            end = mpz_get_ui(mpq_numref(((MinimNumber*) args[2]->data)->rat));
            if (!assert_generic(&res, "Expected [begin, end) where begin < end <= len in 'substring'",
                                start < end && end <= len))
                return res;
        }

        tmp = malloc((end - start + 1) * sizeof(char));
        strncpy(tmp, &str[start], end - start);
        tmp[end - start] = '\0';

        init_minim_object(&res, MINIM_OBJ_STRING, tmp);
    }

    return res;
}

MinimObject *minim_builtin_string_to_symbol(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "string->symbol", 1, argc) &&
        assert_string(args[0], &res, "Expected a string in the 1st argument of 'string->symbol'"))
    {
        char *dest = malloc((strlen(args[0]->data) + 1) * sizeof(char));
        strcpy(dest, args[0]->data);

        init_minim_object(&res, MINIM_OBJ_SYM, dest);
        free(dest);     // this is dumb
    }

    return res;
}

MinimObject *minim_builtin_symbol_to_string(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "symbol->string", 1, argc) &&
        assert_symbol(args[0], &res, "Expected a symbol in the 1st argument of 'symbol->string'"))
    {
        char *dest = malloc((strlen(args[0]->data) + 1) * sizeof(char));
        strcpy(dest, args[0]->data);

        init_minim_object(&res, MINIM_OBJ_STRING, dest);
    }

    return res;
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

MinimObject *minim_builtin_format(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "symbol->string", 1, argc) &&
        assert_string(args[0], &res, "Expected a string in the 1st argument of 'format'"))
    {
        char *str = args[0]->data;
        size_t len = strlen(str);
        size_t vcount = collect_format_vars(str, len);

        if (assert_generic(&res, "Number of format variables do not match argument count", vcount == (argc - 1)))
        {
            Buffer *bf;
            size_t var = 1;

            init_buffer(&bf);
            for (size_t i = 0; i < len; ++i)
            {
                if (str[i] == '~')
                {
                    if (++i == len)
                    {
                        minim_error(&res, "Expected a character after '~'");
                        free_buffer(bf);
                        return res;
                    }
                    else if (str[i] == 'a')
                    {
                        PrintParams pp;
                        char *t;

                        pp.quote = true;
                        pp.display = true;
                        t = print_to_string(args[var++], env, &pp);
                        writes_buffer(bf, t);
                        free(t);
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
            init_minim_object(&res, MINIM_OBJ_STRING, release_buffer(bf));
            free_buffer(bf);
        }
    }

    return res;
}

MinimObject *minim_builtin_printf(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "printf", 1, argc) &&
        assert_string(args[0], &res, "Expected a string for the 1st argument of 'printf'"))
    {
        MinimObject *val = minim_builtin_format(env, args, argc);
        if (val->type != MINIM_OBJ_ERR)
        {
            PrintParams pp;

            pp.quote = true;
            pp.display = true;
            
            replace_special_chars(val->data);
            print_minim_object(val, env, &pp);
            init_minim_object(&res, MINIM_OBJ_VOID);
            free_minim_object(val);
        }
        else
        {
            res = val;
        }

    }

    return res;
}
#include <stdlib.h>
#include <string.h>

#include "common/buffer.h"
#include "assert.h"
#include "number.h"
#include "string.h"

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

// *** Builtins *** //

void env_load_module_string(MinimEnv *env)
{
    env_load_builtin(env, "string?", MINIM_OBJ_FUNC, minim_builtin_stringp);
    env_load_builtin(env, "string-append", MINIM_OBJ_FUNC, minim_builtin_string_append);
    env_load_builtin(env, "substring", MINIM_OBJ_FUNC, minim_builtin_substring);
}

MinimObject *minim_builtin_stringp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "string?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_stringp(args[0]));
        
    return res;
}

MinimObject *minim_builtin_string_append(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, "string-append", 1) &&
        assert_for_all(argc, args, &res, "Expected all string arguments for 'string-append'", minim_stringp))
    {
        Buffer *bf;

        init_buffer(&bf);
        for (int i = 0; i < argc; ++i)
            writes_buffer(bf, args[i]->data);

        trim_buffer(bf);
        init_minim_object(&res, MINIM_OBJ_STRING, release_buffer(bf));
        free_buffer(bf);
    }

    return res;
}

MinimObject *minim_builtin_substring(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_range_argc(2, &res, "substring", 2, 3) &&
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
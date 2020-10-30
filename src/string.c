
#include "common/buffer.h"
#include "assert.h"
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
    env_load_builtin(env, "str-append", MINIM_OBJ_FUNC, minim_builtin_str_append);
}

MinimObject *minim_builtin_stringp(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, &res, "string?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_stringp(args[0]));
        
    return res;
}

MinimObject *minim_builtin_str_append(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_min_argc(argc, &res, "str-append", 1) &&
        assert_for_all(argc, args, &res, "Expected all string arguments for 'str-append'", minim_stringp))
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
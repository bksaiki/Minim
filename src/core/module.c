#include "builtin.h"
#include "error.h"
#include "../minim.h"

MinimObject *minim_builtin_export(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject **syms, *ret;

    syms = GC_alloc(argc * sizeof(MinimObject));
    for (size_t i = 0; i < argc; ++i)
    {
        unsyntax_ast(env, MINIM_AST(args[i]), &syms[i]);
        if (!MINIM_OBJ_SYMBOLP(syms[i]))
        {
            ret = minim_error("export must be a symbol", "%export");
            return ret;
        }
    }

    if (env->prev)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            MinimObject *val = env_get_sym(env, MINIM_STRING(syms[i]));

            if (!val)
            {
                ret = minim_error("unknown identifier", MINIM_STRING(syms[i]));
                return ret;
            }

            env_intern_sym(env->prev, MINIM_STRING(syms[i]), val);
        }
    }

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret, *arg;
    Buffer *path;

    if (!env->current_dir)
    {
        ret = minim_error("environment improperly configured", "%import");
        return ret;
    }
    
    for (size_t i = 0; i < argc; ++i)
    {
        unsyntax_ast(env, MINIM_AST(args[i]), &arg);
        if (!MINIM_OBJ_STRINGP(arg))
        {
            ret = minim_error("import must be a string or symbol", "%import");
            return ret;
        }

        init_buffer(&path);
        writes_buffer(path, env->current_dir);
        writes_buffer(path, MINIM_STRING(arg));

        if (minim_load_file(env, get_buffer(path)))
        {
            init_minim_object(&ret, MINIM_OBJ_VOID);
            return ret;
        }
    }

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

MinimObject *minim_builtin_top_level(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret;

    if (!env->prev)
    {
        ret = minim_error("cannot be used in top environment", "%top-level");
        return ret;
    }

    minim_symbol_table_merge(env->prev->table, env->table);
    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

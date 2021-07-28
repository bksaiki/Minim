#include "builtin.h"
#include "error.h"
#include "../minim.h"

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret, *arg;
    Buffer *path;

    if (!env->current_dir)
    {
        ret = minim_error("environment improperly configured", "%import");
        return ret;
    }

    unsyntax_ast(env, MINIM_AST(args[0]), &arg);
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

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

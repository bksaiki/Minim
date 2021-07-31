#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "module.h"
#include "read.h"

void init_minim_module(MinimModule **pmodule)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;
    module->exprs = NULL;
    module->exprc = 0;
    module->env = NULL;
    module->import = NULL;
    module->flags = 0x0;

    *pmodule = module;
}

void minim_module_add_expr(MinimModule *module, SyntaxNode *expr)
{
    ++module->exprc;
    module->exprs = GC_realloc(module->exprs, module->exprc * sizeof(SyntaxNode*));
    module->exprs[module->exprc - 1] = expr;
}

// ================================ Builtins ================================

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

    if (env->module->prev)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            MinimObject *val = env_get_sym(env, MINIM_STRING(syms[i]));

            if (!val)
            {
                ret = minim_error("unknown identifier", MINIM_STRING(syms[i]));
                return ret;
            }


            env_intern_sym(env->module->prev->env, MINIM_STRING(syms[i]), val);
        }
    }

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret, *arg;
    MinimModule *module2;
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
        
        module2 = minim_load_file_as_module(env->module, get_buffer(path), &ret);
        if (!module2) return ret;

        if (!eval_module(module2, &ret))
            return ret;
        
    }

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

MinimObject *minim_builtin_top_level(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret;

    if (!env->module->prev)
    {
        ret = minim_error("cannot be used in top environment", "%top-level");
        return ret;
    }

    minim_symbol_table_merge(env->module->prev->env->table, env->table);
    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

#include "string.h"

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "module.h"
#include "read.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

void init_minim_module(MinimModule **pmodule, MinimModuleCache *cache)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;
    module->exprs = NULL;
    module->imports = NULL;
    module->cache = cache;
    module->exprc = 0;
    module->importc = 0;
    module->env = NULL;
    init_env(&module->import, NULL, NULL);
    module->name = NULL;
    module->flags = 0x0;

    *pmodule = module;
}

void copy_minim_module(MinimModule **pmodule, MinimModule *src)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;                    // fresh copy
    module->exprs = src->exprs;
    module->imports = NULL;                 // fresh copy
    module->cache = src->cache;
    module->exprc = src->exprc;
    module->importc = 0;                    // fresh copy
    module->env = NULL;                     // fresh copy
    init_env(&module->import, NULL, NULL);  // fresh copy
    module->name = src->name;
    module->flags = 0x0;

    *pmodule = module;
}

void minim_module_add_expr(MinimModule *module, SyntaxNode *expr)
{
    ++module->exprc;
    module->exprs = GC_realloc(module->exprs, module->exprc * sizeof(SyntaxNode*));
    module->exprs[module->exprc - 1] = expr;
}

void minim_module_add_import(MinimModule *module, MinimModule *import)
{
    for (size_t i = 0; i < module->importc; ++i)
    {
        if (strcmp(import->name, module->imports[i]->name) == 0)
            return;
    }
    
    ++module->importc;
    module->imports = GC_realloc(module->imports, module->importc * sizeof(MinimModule*));
    module->imports[module->importc - 1] = import;
}

MinimObject *minim_module_get_sym(MinimModule *module, const char *sym)
{
    return minim_symbol_table_get(module->env->table, sym);
}

MinimModule *minim_module_get_import(MinimModule *module, const char *sym)
{
    for (size_t i = 0; i < module->importc; ++i)
    {
        if (module->imports[i]->name &&
            strcmp(module->imports[i]->name, sym) == 0)
            return module->imports[i];
    }

    return NULL;
}

void init_minim_module_cache(MinimModuleCache **pcache)
{
    MinimModuleCache *cache;

    cache = GC_alloc(sizeof(MinimModuleCache));
    cache->modules = NULL;
    cache->modulec = 0;

    *pcache = cache;
}

void minim_module_cache_add(MinimModuleCache *cache, MinimModule *import)
{
    ++cache->modulec;
    cache->modules = GC_realloc(cache->modules, cache->modulec * sizeof(MinimModule*));
    cache->modules[cache->modulec - 1] = import;
}

MinimModule *minim_module_cache_get(MinimModuleCache *cache, const char *sym)
{
    for (size_t i = 0; i < cache->modulec; ++i)
    {
        if (strcmp(cache->modules[i]->name, sym) == 0)
            return cache->modules[i];
    }

    return NULL;
}

// ================================ Builtins ================================

MinimObject *minim_builtin_export(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret;

    if (env->module->prev)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            MinimObject *export;

            unsyntax_ast(env, MINIM_AST(args[i]), &export);
            if (minim_listp(export))
            {
                MinimModule *import;
                MinimObject *name;
                Buffer *path;
                char *attrib;
                
                attrib = MINIM_AST(MINIM_CAR(export))->sym;
                unsyntax_ast(env, MINIM_AST(MINIM_CADR(export)), &name);
                if (strcmp(attrib, "all") == 0)
                {
                    init_buffer(&path);
                    writes_buffer(path, env->current_dir);
                    writes_buffer(path, MINIM_STRING(name));

                    import = minim_module_get_import(env->module, get_buffer(path));
                    if (!import)
                    {
                        ret = minim_syntax_error("module not imported",
                                                 "%export",
                                                 MINIM_AST(args[i]),
                                                 MINIM_AST(MINIM_CADR(export)));
                        return ret;
                    }

                    minim_symbol_table_merge(env->module->prev->import->table,
                                             import->env->table);
                }
            }
            else
            {
                MinimObject *val;

                val = minim_module_get_sym(env->module, MINIM_STRING(export));
                if (!val)
                {
                    ret = minim_error("identifier not defined in module", MINIM_STRING(export));
                    return ret;
                }

                env_intern_sym(env->module->prev->import, MINIM_STRING(export), val);
            }
        }
    }

    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *ret, *arg;
    MinimModule *tmp, *module2;
    Buffer *path;

    if (!env->current_dir)
    {
        ret = minim_error("environment improperly configured", "%import");
        return ret;
    }

    if (!env->module->cache)
        init_minim_module_cache(&env->module->cache);

    init_minim_module(&tmp, env->module->cache);
    for (size_t i = 0; i < argc; ++i)
    {
        unsyntax_ast(env, MINIM_AST(args[i]), &arg);

        init_buffer(&path);
        writes_buffer(path, env->current_dir);
        writes_buffer(path, MINIM_STRING(arg));

        module2 = minim_module_cache_get(env->module->cache, get_buffer(path));
        if (module2)
        {
            Buffer *mfname, *dir;

            init_buffer(&mfname);
            valid_path(mfname, get_buffer(path));
            dir = get_directory(get_buffer(mfname));

            copy_minim_module(&module2, module2);
            init_env(&module2->env, get_builtin_env(env), NULL);
            module2->env->current_dir = get_buffer(dir);
            module2->env->module = module2;
        }
        else
        {
            module2 = minim_load_file_as_module(env->module, get_buffer(path), &ret);
            if (!module2) return ret;

            module2->name = get_buffer(path);
            minim_module_cache_add(env->module->cache, module2);
        }

        module2->prev = tmp;
        minim_module_add_import(env->module, module2);
        if (!eval_module(module2, &ret))
            return ret;
    }

    minim_symbol_table_merge(env->module->import->table, tmp->import->table);
    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

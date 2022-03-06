#include "minimpriv.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

static MinimModule *module_from_cache(MinimModule *src)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;                    // no copy
    module->imports = NULL;                 // no copy
    module->importc = 0;                    // no copy
    module->env = NULL;                     // no copy
    module->export = src->export;
    module->name = src->name;

    return module;
}

void init_minim_module(MinimModule **pmodule)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;
    module->imports = NULL;
    module->importc = 0;
    module->env = NULL;
    init_env(&module->export, NULL, NULL);
    module->name = NULL;

    // Set up empty body
    module->body = minim_ast(
        minim_cons(minim_ast(intern("%module"), NULL),
        minim_cons(minim_ast(minim_false, NULL),
        minim_cons(minim_ast(minim_false, NULL),
        minim_cons(minim_ast(
            minim_cons(minim_ast(intern("%module-begin"), NULL),
            minim_null), NULL),
        minim_null)))), NULL);

    *pmodule = module;
}

void minim_module_add_expr(MinimModule *module, MinimObject *expr)
{
    MinimObject *t = MINIM_MODULE_BODY(module);
    while (!minim_nullp(MINIM_CDR(t)))
        t = MINIM_CDR(t);
    MINIM_CDR(t) = minim_cons(expr, minim_null);
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
    size_t hash;

    hash = hash_symbol(sym);
    return minim_symbol_table_get(module->env->table, sym, hash);
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
    if (env->module->prev)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            MinimObject *export;

            export = MINIM_STX_VAL(args[i]);
            if (minim_listp(export))
            {
                MinimModule *import;
                MinimObject *name;
                MinimPath *path;
                char *attrib;
                
                attrib = MINIM_STX_SYMBOL(MINIM_CAR(export));
                name = MINIM_STX_VAL(MINIM_CADR(export));
                if (strcmp(attrib, "all") == 0)
                {
                    path = (is_absolute_path(MINIM_STRING(name)) ?
                            build_path(1, MINIM_STRING(name)) :
                            build_path(2, env->current_dir, MINIM_STRING(name)));

                    import = minim_module_get_import(env->module, extract_path(path));
                    if (!import)
                    {
                        THROW(env, minim_syntax_error("module not imported",
                                                      "%export",
                                                      args[i],
                                                      MINIM_CADR(export)));
                    }

                    minim_symbol_table_merge(env->module->export->table, import->export->table);
                }
            }
            else
            {
                MinimObject *val;

                val = minim_module_get_sym(env->module, MINIM_SYMBOL(export));
                if (!val)
                {
                    THROW(env, minim_error("identifier not defined in module",
                                           MINIM_SYMBOL(export)));
                }

                env_intern_sym(env->module->export, MINIM_SYMBOL(export), val);
            }
        }
    }

    return minim_void;
}

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *arg;
    MinimModule *tmp, *module2;
    MinimPath *path;
    char *clean_path;

    if (!env->current_dir)
    {
        printf("environment improperly configured\n");
        return NULL; // panic
    }

    init_minim_module(&tmp);
    for (size_t i = 0; i < argc; ++i)
    {
        arg = MINIM_STX_VAL(args[i]);
        path = (is_absolute_path(MINIM_STRING(arg)) ?
                build_path(1, MINIM_STRING(arg)) :
                build_path(2, env->current_dir, MINIM_STRING(arg)));

        clean_path = extract_path(path);
        module2 = minim_module_cache_get(global.cache, clean_path);
        if (module2)
        {
            module2 = module_from_cache(module2);
            init_env(&module2->env, get_builtin_env(env), NULL);
            module2->env->current_dir = extract_directory(path);
            module2->env->module = module2;
            minim_module_add_import(env->module, module2);
        }
        else
        {
            module2 = minim_load_file_as_module(env->module, clean_path);
            module2->prev = tmp;
            module2->name = clean_path;

            minim_module_cache_add(global.cache, module2);
            minim_module_add_import(env->module, module2);
            eval_module(module2);
        }

        minim_symbol_table_merge(env->table, module2->export->table);
    }

    return minim_void;
}

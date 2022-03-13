#include "minimpriv.h"

#define ASSERT_VALID_KEY(name, path)            \
    if (!name && !path)                         \
    {                                           \
        printf("module cache key is empty");    \
        return NULL;                            \
    }

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

static bool
module_cache_key_match(MinimModule *module,
                      const char *name,
                      const char *path)
{
    if (name)
    {
        if (!module->name || strcmp(module->name, name) != 0)
            return false;
    }

    if (path)
    {
        if (!module->path || strcmp(module->path, path) != 0)
            return false;
    }

    return true;
}

void init_minim_module(MinimModule **pmodule)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->cache = NULL;
    module->imports = NULL;
    module->importc = 0;
    module->export = init_env(NULL);
    module->name = NULL;
    module->path = NULL;
    module->body = NULL;

    *pmodule = module;
}

void minim_module_add_expr(MinimModule *module, MinimObject *expr)
{
    MinimObject *t;

    if (module->body == NULL)
    {
        // Set up empty body
        module->body = minim_ast(
            minim_cons(minim_ast(intern("%module"), NULL),
            minim_cons(minim_ast(minim_false, NULL),
            minim_cons(minim_ast((module->path ? minim_string(module->path) : minim_false), NULL),
            minim_cons(minim_ast(
                minim_cons(minim_ast(intern("%module-begin"), NULL),
                minim_null), NULL),
            minim_null)))), NULL);
    }

    t = MINIM_MODULE_BODY(module);
    while (!minim_nullp(MINIM_CDR(t)))
        t = MINIM_CDR(t);
    MINIM_CDR(t) = minim_cons(expr, minim_null);
}

void minim_module_add_import(MinimModule *module, MinimModule *import)
{
    for (size_t i = 0; i < module->importc; ++i)
    {
        if (module_cache_key_match(module->imports[i], import->name, import->path))
            return;
    }
    
    ++module->importc;
    module->imports = GC_realloc(module->imports, module->importc * sizeof(MinimModule*));
    module->imports[module->importc - 1] = import;
}

void minim_module_set_path(MinimModule *module, const char *name)
{
    module->path = GC_alloc_atomic((strlen(name) + 1) * sizeof(char));
    strcpy(module->path, name);
    if (module->body != NULL)
    {
        MinimObject *path_stx = MINIM_CDDR(MINIM_STX_VAL(module->body));
        MINIM_CAR(path_stx) = minim_ast(minim_string(module->path), NULL);
    }   
}

void init_minim_module_instance(MinimModuleInstance **pinst, MinimModule *module)
{
    MinimModuleInstance *inst;

    inst = GC_alloc(sizeof(MinimModuleInstance));
    inst->module = module;
    inst->prev = NULL;
    inst->env = NULL;

    *pinst = inst;
}

MinimObject *minim_module_get_sym(MinimModuleInstance *module, const char *sym)
{
    return env_get_local_sym(module->env, sym);
}

MinimModule *minim_module_get_import(MinimModule *module, const char *name, const char *path)
{
    ASSERT_VALID_KEY(name, path);
    for (size_t i = 0; i < module->importc; ++i)
    {
        if (module_cache_key_match(module->imports[i], name, path))
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

MinimModule *minim_module_cache_get(MinimModuleCache *cache, const char *name, const char *path)
{
    ASSERT_VALID_KEY(name, path);
    for (size_t i = 0; i < cache->modulec; ++i)
    {
        if (module_cache_key_match(cache->modules[i], name, path))
            return cache->modules[i];
    }

    return NULL;
}

// ================================ Builtins ================================

MinimObject *minim_builtin_export(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (env->module_inst->prev)
    {
        for (size_t i = 0; i < argc; ++i)
        {
            MinimModule *module;
            MinimObject *export;

            module = env->module_inst->module;
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

                    import = minim_module_get_import(module, NULL, extract_path(path));
                    if (!import)
                    {
                        THROW(env, minim_syntax_error("module not imported",
                                                      "%export",
                                                      args[i],
                                                      MINIM_CADR(export)));
                    }

                    env_merge_local_symbols(module->export, import->export);
                }
            }
            else
            {
                MinimObject *val;

                val = minim_module_get_sym(env->module_inst, MINIM_SYMBOL(export));
                if (!val)
                {
                    THROW(env, minim_error("identifier not defined in module",
                                           MINIM_SYMBOL(export)));
                }

                env_intern_sym(module->export, MINIM_SYMBOL(export), val);
            }
        }
    }

    GC_REGISTER_LOCAL_ARRAY(args);
    return minim_void;
}

MinimObject *minim_builtin_import(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *arg;
    MinimModule *module;
    MinimModuleInstance *module_inst;
    MinimPath *path;
    char *clean_path;

    if (!env->current_dir)
    {
        printf("environment improperly configured\n");
        return NULL; // panic
    }

    for (size_t i = 0; i < argc; ++i)
    {
        arg = MINIM_STX_VAL(args[i]);
        path = (is_absolute_path(MINIM_STRING(arg)) ?
                build_path(1, MINIM_STRING(arg)) :
                build_path(2, env->current_dir, MINIM_STRING(arg)));

        clean_path = extract_path(path);
        module = minim_module_cache_get(global.cache, NULL, clean_path);
        if (module)
        {
            init_minim_module_instance(&module_inst, module);
            module_inst->env = init_env(get_builtin_env(env));
            module_inst->env->current_dir = extract_directory(path);
            module_inst->env->module_inst = module_inst;
        }
        else
        {
            MinimModule *empty;
            MinimModuleInstance *empty_inst;

            init_minim_module(&empty);
            init_minim_module_instance(&empty_inst, empty);

            module_inst = minim_load_file_as_module(env->module_inst, clean_path);
            module_inst->prev = empty_inst;

            minim_module_cache_add(global.cache, module_inst->module);
            eval_module(module_inst);
        }

        minim_module_add_import(env->module_inst->module, module_inst->module);
        env_merge_local_symbols(env, module_inst->module->export);
    }

    GC_REGISTER_LOCAL_ARRAY(args);
    return minim_void;
}

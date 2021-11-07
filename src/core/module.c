#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "global.h"
#include "hash.h"
#include "list.h"
#include "module.h"
#include "read.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

static bool expr_is_begin(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;

    if (!MINIM_STX_PAIRP(stx) || !MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        return false;

    val = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_begin);
}

static MinimModule *module_from_cache(MinimModule *src)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;                    // no copy
    module->exprs = src->exprs;
    module->imports = NULL;                 // no copy
    module->exprc = src->exprc;
    module->importc = 0;                    // no copy
    module->env = NULL;                     // no copy
    module->export = src->export;
    module->name = src->name;
    module->flags = 0x0;

    return module;
}

void init_minim_module(MinimModule **pmodule)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;
    module->exprs = NULL;
    module->imports = NULL;
    module->exprc = 0;
    module->importc = 0;
    module->env = NULL;
    init_env(&module->export, NULL, NULL);
    module->name = NULL;
    module->flags = 0x0;

    *pmodule = module;
}

void minim_module_add_expr(MinimModule *module, MinimObject *expr)
{
    ++module->exprc;
    module->exprs = GC_realloc(module->exprs, module->exprc * sizeof(MinimObject*));
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

static size_t expand_expr_count(MinimEnv *env, MinimObject *stx)
{
    if (expr_is_begin(env, stx))
    {
        size_t count = 0;
        for (MinimObject *it = MINIM_STX_CDR(stx); MINIM_STX_PAIRP(it); it = MINIM_CDR(it))
            count += expand_expr_count(env, MINIM_CAR(it));
        return count;
    }
    else
    {
        return 1;
    }
}

static size_t expand_expr(MinimEnv *env, MinimObject *stx, MinimObject **expanded, size_t idx)
{
    if (expr_is_begin(env, stx))
    {
        for (MinimObject *it = MINIM_STX_CDR(stx); MINIM_STX_PAIRP(it); it = MINIM_CDR(it))
            idx = expand_expr(env, MINIM_CAR(it), expanded, idx);
    }
    else
    {
        expanded[idx] = stx;
        ++idx;
    }

    return idx;
}

// expands begin
void minim_module_expand(MinimModule *module)
{
    MinimObject **expanded;
    size_t it = 0, count = 0;
    
    for (size_t i = 0; i < module->exprc; ++i)
        count += expand_expr_count(module->env, module->exprs[i]);
        
    expanded = GC_alloc(count * sizeof(MinimObject**));
    for (size_t i = 0; i < module->exprc; ++i)
        it = expand_expr(module->env, module->exprs[i], expanded, it);
    
    module->exprs = expanded;
    module->exprc = count;
}

MinimObject *minim_module_get_sym(MinimModule *module, const char *sym)
{
    size_t hash;

    hash = hash_bytes(sym, strlen(sym));
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

            export = unsyntax_ast(env, MINIM_STX_VAL(args[i]));
            if (minim_listp(export))
            {
                MinimModule *import;
                MinimObject *name;
                MinimPath *path;
                char *attrib;
                
                attrib = MINIM_STX_SYMBOL(MINIM_CAR(export));
                name = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CADR(export)));
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
                                                      MINIM_STX_VAL(args[i]),
                                                      MINIM_STX_VAL(MINIM_CADR(export))));
                    }

                    minim_symbol_table_merge(env->module->export->table, import->export->table);
                }
            }
            else
            {
                MinimObject *val;

                val = minim_module_get_sym(env->module, MINIM_STRING(export));
                if (!val)
                    THROW(env, minim_error("identifier not defined in module", MINIM_STRING(export)));

                env_intern_sym(env->module->export, MINIM_STRING(export), val);
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
        arg = unsyntax_ast(env, MINIM_STX_VAL(args[i]));
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

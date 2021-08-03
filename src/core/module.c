#include "string.h"

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "module.h"
#include "read.h"

void init_minim_module(MinimModule **pmodule)
{
    MinimModule *module;

    module = GC_alloc(sizeof(MinimModule));
    module->prev = NULL;
    module->exprs = NULL;
    module->exprc = 0;
    module->imports = NULL;
    module->importc = 0;
    module->env = NULL;
    init_env(&module->import, NULL, NULL);
    module->name = NULL;
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

    init_minim_module(&tmp);
    for (size_t i = 0; i < argc; ++i)
    {
        unsyntax_ast(env, MINIM_AST(args[i]), &arg);

        init_buffer(&path);
        writes_buffer(path, env->current_dir);
        writes_buffer(path, MINIM_STRING(arg));
        
        module2 = minim_load_file_as_module(env->module, get_buffer(path), &ret);
        if (!module2) return ret;

        module2->prev = tmp;
        module2->name = get_buffer(path);
        if (!eval_module(module2, &ret))
            return ret;
        
        minim_module_add_import(env->module, module2);
    }

    minim_symbol_table_merge(env->module->import->table, tmp->import->table);
    init_minim_object(&ret, MINIM_OBJ_VOID);
    return ret;
}

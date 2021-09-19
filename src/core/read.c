#include "../minim.h"
#include "error.h"

#define CACHE_OR_NULL(env)      ((env)->module ? (env)->module->cache : NULL)

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

static char *directory_from_port(MinimObject *port)
{
    MinimPath *path = build_path(1, MINIM_PORT_NAME(port));
    return extract_directory(path);
}

static MinimObject *open_file_port(MinimEnv *env, const char *fname)
{
    MinimObject *port;
    MinimPath *path;
    FILE *file;
    char *clean_path;

    path = build_relative_path(1, fname);
    clean_path = extract_path(path);
    file = fopen(clean_path, "r");
    if (!file)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "Could not open file \"~s\"", clean_path);
        THROW(env, minim_error(get_buffer(bf), NULL));
    }

    port = minim_file_port(file, MINIM_PORT_MODE_READ |
                                 MINIM_PORT_MODE_OPEN |
                                 MINIM_PORT_MODE_READY);
    MINIM_PORT_NAME(port) = clean_path;
    return port;
}

static void emit_processed_file(MinimObject *fport, MinimModule *module)
{
#if defined(MINIM_LINUX)            // only enabled for linux
    MinimPath *fname, *cname;
    FILE *cfile;
    
    fname = build_path(1, MINIM_PORT_NAME(fport));
    cname = build_path(2, extract_directory(fname), ".cache");
    make_directory(extract_path(fname));         // TODO: abort if failed

    path_append(cname, extract_file(fname));
    cfile = fopen(extract_path(cname), "w");
    for (size_t i = 0; i < module->exprc; ++i)
    {
        print_ast_to_port(module->exprs[i], cfile);
        fputc('\n', cfile);
    }
    
    fclose(cfile);
#endif
}

static MinimObject *read_error(MinimObject *port, SyntaxNode *err, const char *fname)
{
    MinimError *e;
    Buffer *bf;

    init_buffer(&bf);
    writef_buffer(bf, "~s:~u:~u", fname, MINIM_PORT_ROW(port), MINIM_PORT_COL(port));

    init_minim_error(&e, "bad syntax", err->sym);
    init_minim_error_desc_table(&e->table, 1);
    minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
    return minim_err(e);
}

MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname)
{
    MinimModule *module;
    MinimObject *port;

    port = open_file_port(prev->env, fname);
    init_minim_module(&module, prev->cache);
    init_env(&module->env, get_builtin_env(prev->env), NULL);
    module->env->current_dir = directory_from_port(port);
    module->env->module = module;
    module->cache = prev->cache;

    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;

        ast = minim_parse_port(port, &err, 0);
        if (!ast) THROW(prev->env, read_error(port, err, fname));
        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    eval_module_macros(module);
    emit_processed_file(port, module);
    return module;
}

void minim_load_file(MinimEnv *env, const char *fname)
{
    MinimModule *module;
    MinimObject *port;
    
    port = open_file_port(env, fname);
    init_minim_module(&module, CACHE_OR_NULL(env));
    init_env(&module->env, get_builtin_env(env), NULL);
    module->env->current_dir = directory_from_port(port);
    module->env->module = module;

    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;

        ast = minim_parse_port(port, &err, 0);
        if (!ast) THROW(env, read_error(port, err, fname));
        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    eval_module_macros(module);
    emit_processed_file(port, module);
    eval_module(module);
}

void minim_run_file(MinimEnv *env, const char *fname)
{
    MinimModule *module, *prev;
    MinimModuleCache *cache;
    MinimObject *port;
    char *prev_dir;

    prev_dir = env->current_dir;
    prev = env->module;

    port = open_file_port(env, fname);
    init_minim_module_cache(&cache);
    init_minim_module(&module, cache);
    module->env = env;
    env->current_dir = directory_from_port(port);
    env->module = module;

    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;
        
        ast = minim_parse_port(port, &err, 0);
        if (!ast)
        {
            env->current_dir = prev_dir;
            env->module = prev;
            THROW(env, read_error(port, err, fname));
        }

        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    eval_module_macros(module);
    emit_processed_file(port, module);
    eval_module(module);

    // this is dumb
    minim_symbol_table_merge(env->table, module->export->table);
    env->current_dir = prev_dir;
    env->module = prev;
}

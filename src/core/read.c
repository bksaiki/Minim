#include "../minim.h"
#include "error.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname)
{
    PrintParams pp;
    MinimModule *module;
    MinimPath *path;
    MinimObject *port;
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
        THROW(prev->env, minim_error(get_buffer(bf), NULL));
        return NULL;
    }

    port = minim_file_port(file, MINIM_PORT_MODE_READ |
                                 MINIM_PORT_MODE_OPEN |
                                 MINIM_PORT_MODE_READY);
    MINIM_PORT_NAME(port) = clean_path;

    init_minim_module(&module, prev->cache);
    init_env(&module->env, get_builtin_env(prev->env), NULL);
    module->env->current_dir = extract_directory(path);
    module->env->module = module;
    module->cache = prev->cache;

    set_default_print_params(&pp);
    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;

        ast = minim_parse_port(port, &err, 0);
        if (!ast)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, MINIM_PORT_ROW(port), MINIM_PORT_COL(port));

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
            THROW(prev->env, minim_err(e));
        }

        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    return module;
}

void minim_load_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    MinimPath *path;
    MinimModule *module;
    MinimObject *port;
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

    init_minim_module(&module, (env->module ? env->module->cache : NULL));
    init_env(&module->env, get_builtin_env(env), NULL);
    module->env->current_dir = extract_directory(path);
    module->env->module = module;

    set_default_print_params(&pp);
    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;

        ast = minim_parse_port(port, &err, 0);
        if (!ast)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, MINIM_PORT_ROW(port), MINIM_PORT_COL(port));

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
            THROW(env, minim_err(e));
        }

        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    eval_module(module);
}

void minim_run_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    MinimModule *module, *prev;
    MinimModuleCache *cache;
    MinimObject *port;
    MinimPath *path;
    FILE *file;
    char *prev_dir, *clean_path;

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
    
    prev_dir = env->current_dir;
    prev = env->module;

    init_minim_module_cache(&cache);
    init_minim_module(&module, cache);
    module->env = env;
    env->current_dir = extract_directory(path);
    env->module = module;

    set_default_print_params(&pp);
    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        SyntaxNode *ast, *err;
        
        ast = minim_parse_port(port, &err, 0);
        if (!ast)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, MINIM_PORT_ROW(port), MINIM_PORT_COL(port));

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));

            env->current_dir = prev_dir;
            env->module = prev;
            THROW(env, minim_err(e));
        }

        minim_module_add_expr(module, ast);
    }

    minim_module_expand(module);
    eval_module(module);

    // this is dumb
    minim_symbol_table_merge(env->table, module->export->table);
    env->current_dir = prev_dir;
    env->module = prev;
}

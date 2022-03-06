#include "minimpriv.h"
#include "../compiler/compile.h"

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

static MinimObject *read_error(MinimObject *port, MinimObject *err, const char *fname)
{
    MinimError *e;
    Buffer *bf;

    init_buffer(&bf);
    writef_buffer(bf, "~s:~u:~u", fname, MINIM_PORT_ROW(port), MINIM_PORT_COL(port));

    init_minim_error(&e, "bad syntax", MINIM_SYMBOL(err));
    init_minim_error_desc_table(&e->table, 1);
    minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
    return minim_err(e);
}

static void emit_code_file(MinimObject *fport, Buffer *code)
{
#if defined(MINIM_LINUX)            // only enabled for linux
    MinimPath *fname, *cname;
    Buffer *bf;
    FILE *cfile;
    
    fname = build_path(1, MINIM_PORT_NAME(fport));
    cname = build_path(2, extract_directory(fname), ".cache");
    make_directory(extract_path(cname));         // TODO: abort if failed

    init_buffer(&bf);
    writes_buffer(bf, extract_file(fname));
    writec_buffer(bf, 'o');
    path_append(cname, get_buffer(bf));

    cfile = fopen(extract_path(cname), "w");
    fputs(get_buffer(code), cfile);
    fclose(cfile);
#endif
}

// ================================ Public ================================

MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname)
{
    MinimModule *module;
    MinimObject *port, *cache;

    port = open_file_port(prev->env, fname);
    cache = load_processed_file(port);
    if (cache)
    {
        init_minim_module(&module);
        init_env(&module->env, get_builtin_env(prev->env), NULL);
        module->env->current_dir = directory_from_port(cache);
        module->env->module = module;
        while (MINIM_PORT_MODE(cache) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;

            ast = minim_parse_port(cache, &err, 0);
            if (err != NULL)
                THROW(prev->env, read_error(cache, err, fname));
            minim_module_add_expr(module, ast);
        }

        eval_module_cached(module);
        return module;
    }
    else
    {
        init_minim_module(&module);
        init_env(&module->env, get_builtin_env(prev->env), NULL);
        module->env->current_dir = directory_from_port(port);
        module->env->module = module;

        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;

            ast = minim_parse_port(port, &err, 0);
            if (err != NULL)
                THROW(prev->env, read_error(port, err, fname));
            minim_module_add_expr(module, ast);
        }

        expand_minim_module(module->env, module);
        printf("expanded %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        printf("evaluating %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        eval_module_macros(module);
        emit_processed_file(port, module);
        return module;
    }
}

void minim_load_file(MinimEnv *env, const char *fname)
{
    MinimModule *module;
    MinimObject *port, *cache;
    Buffer *code;
    
    port = open_file_port(env, fname);
    cache = load_processed_file(port);
    if (cache)
    {
        init_minim_module(&module);
        init_env(&module->env, get_builtin_env(env), NULL);
        module->env->current_dir = directory_from_port(cache);
        module->env->module = module;

        while (MINIM_PORT_MODE(cache) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;

            ast = minim_parse_port(cache, &err, 0);
            if (err != NULL)
                THROW(env, read_error(cache, err, fname));
            minim_module_add_expr(module, ast);
        }

        eval_module_cached(module);
    }
    else
    {
        init_minim_module(&module);
        init_env(&module->env, get_builtin_env(env), NULL);
        module->env->current_dir = directory_from_port(port);
        module->env->module = module;

        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;

            ast = minim_parse_port(port, &err, 0);
            if (err != NULL)
                THROW(env, read_error(port, err, fname));
            minim_module_add_expr(module, ast);
        }

        expand_minim_module(module->env, module);
        printf("expanded %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        printf("evaluating %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        eval_module_macros(module);
        emit_processed_file(port, module);
    }

    // compile
    if (global.flags & GLOBAL_FLAG_COMPILE)
    {
        code = compile_module(env, module);
        emit_code_file(port, code);
    }

    // eval
    eval_module(module);
}

void minim_run_file(MinimEnv *env, const char *fname)
{
    MinimModule *module, *prev;
    MinimObject *port, *cport;
    char *prev_dir;

    prev_dir = env->current_dir;
    prev = env->module;
    port = open_file_port(env, fname);
    cport = load_processed_file(port);
    if (cport)
    {
        init_minim_module(&module);
        module->env = env;
        env->current_dir = directory_from_port(cport);
        env->module = module;

        while (MINIM_PORT_MODE(cport) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;
            
            ast = minim_parse_port(cport, &err, 0);
            if (err != NULL)
            {
                env->current_dir = prev_dir;
                env->module = prev;
                THROW(env, read_error(cport, err, fname));
            }

            minim_module_add_expr(module, ast);
        }

        eval_module_cached(module);
    }
    else
    {
        init_minim_module(&module);
        module->env = env;
        env->current_dir = directory_from_port(port);
        env->module = module;

        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;
            
            ast = minim_parse_port(port, &err, 0);
            if (err != NULL)
            {
                env->current_dir = prev_dir;
                env->module = prev;
                THROW(env, read_error(port, err, fname));
            }

            minim_module_add_expr(module, ast);
        }

        expand_minim_module(module->env, module);
        printf("expanded %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        printf("evaluating %s: ", fname);
        print_syntax_to_port(module->body, stdout);
        printf("\n\n");

        eval_module_macros(module);
        emit_processed_file(port, module);
    }

    eval_module(module);

    // this is dumb
    minim_symbol_table_merge(env->table, module->export->table);
    env->current_dir = prev_dir;
    env->module = prev;
}

MinimObject *load_processed_file(MinimObject *fport)
{
#if defined(MINIM_LINUX)            // only enabled for linux
    MinimPath *fname, *cname;
    time_t *flast, *clast;
    MinimObject *port;
    FILE *cfile;
    char *name;

    if (global.flags & GLOBAL_FLAG_CACHE)
    {
        fname = build_path(1, MINIM_PORT_NAME(fport));
        cname = build_path(2, extract_directory(fname), ".cache");
        path_append(cname, extract_file(fname));
        
        name = extract_path(cname);
        cfile = fopen(name, "r");
        if (!cfile)     return NULL;

        flast = get_last_modified(MINIM_PORT_NAME(fport));
        clast = get_last_modified(name);
        if (difftime(*flast, *clast) > 0)       // file is newer than cache
            return NULL;

        // printf("loading from cache: %s\n", MINIM_PORT_NAME(fport));
        port = minim_file_port(cfile, MINIM_PORT_MODE_READ |
                                    MINIM_PORT_MODE_OPEN |
                                    MINIM_PORT_MODE_READY);
        MINIM_PORT_NAME(port) = MINIM_PORT_NAME(fport);
        return port;
    }
    else
    {
        return NULL;
    }
#endif
    return NULL;
}

void emit_processed_file(MinimObject *fport, MinimModule *module)
{
#if defined(MINIM_LINUX)            // only enabled for linux
    MinimPath *fname, *cname;
    FILE *cfile;

    if (global.flags & GLOBAL_FLAG_CACHE)
    {
        fname = build_path(1, MINIM_PORT_NAME(fport));
        cname = build_path(2, extract_directory(fname), ".cache");
        make_directory(extract_path(cname));         // TODO: abort if failed

        path_append(cname, extract_file(fname));
        cfile = fopen(extract_path(cname), "w");
        print_syntax_to_port(module->body, cfile);
        fclose(cfile);
    }
#endif
}

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

MinimModuleInstance *minim_load_file_as_module(MinimModuleInstance *prev, const char *fname)
{
    MinimModule *module;
    MinimModuleInstance *module_inst;
    MinimObject *port, *cache;
    MinimEnv *builtin_env;

    port = open_file_port(prev->env, fname);
    cache = load_processed_file(port);
    builtin_env = get_builtin_env(prev->env);

    init_minim_module(&module);
    init_minim_module_instance(&module_inst, module);
    init_env(&module_inst->env, builtin_env, NULL);
    module_inst->env->current_dir = directory_from_port(port);
    module_inst->env->module_inst = module_inst;

    if (cache)
    {
        MinimObject *ast, *err;

        if (!(MINIM_PORT_MODE(cache) & MINIM_PORT_MODE_READY))
            THROW(prev->env, read_error(cache, minim_error("unexpected error while reading from cache", NULL), fname));

        ast = minim_parse_port(cache, &err, 0);
        if (err != NULL)
            THROW(prev->env, read_error(cache, err, fname));

        if (MINIM_PORT_MODE(cache) & MINIM_PORT_MODE_READY)
            THROW(prev->env, read_error(cache, minim_error("cached modules should be a single expression", NULL), fname));

        module->body = ast;
        minim_module_set_path(module, fname);
        check_syntax(module_inst->env, module->body);
        eval_module_cached(module_inst);
    }
    else
    {
        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;

            ast = minim_parse_port(port, &err, 0);
            if (err != NULL)
                THROW(prev->env, read_error(port, err, fname));
            minim_module_add_expr(module, ast);
        }

        minim_module_set_path(module, fname);
        check_syntax(module_inst->env, module->body);
        expand_minim_module(module_inst->env, module);
        emit_processed_file(port, module);

        // compile
        if (global.flags & GLOBAL_FLAG_COMPILE)
        {
            compile_module(module_inst->env, module);
            // emit_code_file(port, code);
        }
    }

    return module_inst;
}

void minim_load_file(MinimEnv *env, const char *fname)
{
    MinimModule *module;
    MinimModuleInstance *module_inst;
    MinimObject *port;
    MinimEnv *env2;
    
    // Always re-read and run

    init_minim_module(&module);
    init_minim_module_instance(&module_inst, module);
    init_env(&env2, get_builtin_env(env), NULL);
    module_inst->env = env2;
    env2->module_inst = module_inst;

    // read
    port = open_file_port(env, fname);
    env2->current_dir = directory_from_port(port);
    while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
    {
        MinimObject *ast, *err;

        ast = minim_parse_port(port, &err, 0);
        if (err != NULL)
            THROW(env, read_error(port, err, fname));
        minim_module_add_expr(module, ast);
    }

    minim_module_set_path(module, fname);
    check_syntax(module_inst->env, module->body);
    expand_minim_module(env2, module);

    // emit desugared program
    emit_processed_file(port, module);

    // compile
    if (global.flags & GLOBAL_FLAG_COMPILE)
    {
        compile_module(module_inst->env, module);
        // emit_code_file(port, code);
    }

    // eval
    eval_module(module_inst);
}

void minim_run_file(MinimEnv *env, const char *fname)
{
    MinimModuleInstance *module_inst, *prev;
    MinimModule *module;
    MinimObject *port, *cport;
    char *prev_dir;

    prev_dir = env->current_dir;
    prev = env->module_inst;
    port = open_file_port(env, fname);
    cport = load_processed_file(port);
    if (cport)
    {
        MinimObject *ast, *err;

        init_minim_module(&module);
        env->current_dir = directory_from_port(cport);

        if (!(MINIM_PORT_MODE(cport) & MINIM_PORT_MODE_READY))
            THROW(env, read_error(cport, minim_error("unexpected error while reading from cache", NULL), fname));

        ast = minim_parse_port(cport, &err, 0);
        if (err != NULL)
        {
            env->current_dir = prev_dir;
            env->module_inst = prev;
            THROW(env, read_error(cport, err, fname));
        }

        if (MINIM_PORT_MODE(cport) & MINIM_PORT_MODE_READY)
            THROW(env, read_error(cport, minim_error("cached modules should be a single expression", NULL), fname));
        
        init_minim_module_instance(&module_inst, module);
        minim_module_set_path(module, fname);
        module_inst->env = env;
        module->body = ast;
        env->module_inst = module_inst;

        check_syntax(module_inst->env, module->body);
        eval_module_cached(module_inst);
    }
    else
    {
        init_minim_module(&module);
        env->current_dir = directory_from_port(port);

        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            MinimObject *ast, *err;
            
            ast = minim_parse_port(port, &err, 0);
            if (err != NULL)
            {
                env->current_dir = prev_dir;
                env->module_inst = prev;
                THROW(env, read_error(port, err, fname));
            }

            minim_module_add_expr(module, ast);
        }

        init_minim_module_instance(&module_inst, module);
        minim_module_set_path(module, fname);
        module_inst->env = env;
        env->module_inst = module_inst;
        
        check_syntax(module_inst->env, module->body);
        expand_minim_module(env, module);
        emit_processed_file(port, module);

        // compile
        if (global.flags & GLOBAL_FLAG_COMPILE)
        {
            compile_module(module_inst->env, module);
            // emit_code_file(port, code);
        }
    }

    // evaluate
    eval_module(module_inst);

    // this is dumb
    env_merge_local_symbols(env, module->export);
    env->current_dir = prev_dir;
    env->module_inst = prev;
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

#include "../minim.h"
#include "error.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname, MinimObject **perr)
{
    PrintParams pp;
    ReadTable rt;
    Buffer *mfname, *dir;
    MinimModule *module;
    FILE *file;

    init_buffer(&mfname);
    valid_path(mfname, fname);
    file = fopen(mfname->data, "r");

    if (!file)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "Could not open file \"~s\"", mfname->data);
        *perr = minim_error(get_buffer(bf), NULL);
        return NULL;
    }

    rt.idx = 0;
    rt.row = 1;
    rt.col = 0;
    rt.flags = 0x0;
    rt.eof = EOF;

    dir = get_directory(get_buffer(mfname));

    init_minim_module(&module);
    init_env(&module->env, get_builtin_env(prev->env), NULL);
    module->env->current_dir = get_buffer(dir);
    module->env->module = module;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        SyntaxNode *ast, *err;

        minim_parse_port(file, fname, &ast, &err, &rt);
        if (!ast || rt.flags & READ_TABLE_FLAG_BAD)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, rt.row, rt.col);

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
            init_minim_object(perr, MINIM_OBJ_ERR, e);

            fclose(file);
            return NULL;
        }

        minim_module_add_expr(module, ast);
    }

    fclose(file);
    return module;
}

int minim_load_file(MinimEnv *env, const char *fname, MinimObject **perr)
{
    PrintParams pp;
    ReadTable rt;
    Buffer *mfname, *dir;
    MinimModule *module;
    FILE *file;

    init_buffer(&mfname);
    valid_path(mfname, fname);
    file = fopen(mfname->data, "r");

    if (!file)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "Could not open file \"~s\"", mfname->data);
        *perr = minim_error(get_buffer(bf), NULL);
        return 1;
    }

    rt.idx = 0;
    rt.row = 1;
    rt.col = 0;
    rt.flags = 0x0;
    rt.eof = EOF;

    dir = get_directory(get_buffer(mfname));

    init_minim_module(&module);
    init_env(&module->env, get_builtin_env(env), NULL);
    module->env->current_dir = get_buffer(dir);
    module->env->module = module;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        SyntaxNode *ast, *err;

        minim_parse_port(file, fname, &ast, &err, &rt);
        if (!ast || rt.flags & READ_TABLE_FLAG_BAD)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, rt.row, rt.col);

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
            init_minim_object(perr, MINIM_OBJ_ERR, e);

            fclose(file);
            return 2;
        }

        minim_module_add_expr(module, ast);
    }

    fclose(file);
    return (eval_module(module, perr) ? 0 : 3);
}

int minim_run_file(MinimEnv *env, const char *fname, MinimObject **perr)
{
    PrintParams pp;
    ReadTable rt;
    Buffer *mfname, *dir;
    MinimModule *module, *prev;
    FILE *file;
    char *prev_dir;
    int status;

    init_buffer(&mfname);
    valid_path(mfname, fname);
    file = fopen(mfname->data, "r");

    if (!file)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "Could not open file \"~s\"", mfname->data);
        *perr = minim_error(get_buffer(bf), NULL);
        return 1;
    }

    rt.idx = 0;
    rt.row = 1;
    rt.col = 0;
    rt.flags = 0x0;
    rt.eof = EOF;

    dir = get_directory(get_buffer(mfname));
    prev_dir = env->current_dir;
    prev = env->module;

    init_minim_module(&module);
    module->env = env;
    env->current_dir = get_buffer(dir);
    env->module = module;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        SyntaxNode *ast, *err;

        minim_parse_port(file, fname, &ast, &err, &rt);
        if (!ast || rt.flags & READ_TABLE_FLAG_BAD)
        {
            MinimError *e;
            Buffer *bf;

            init_buffer(&bf);
            writef_buffer(bf, "~s:~u:~u", fname, rt.row, rt.col);

            init_minim_error(&e, "bad syntax", err->sym);
            init_minim_error_desc_table(&e->table, 1);
            minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
            init_minim_object(perr, MINIM_OBJ_ERR, e);

            fclose(file);
            env->current_dir = prev_dir;
            env->module = prev;
            return 2;
        }

        minim_module_add_expr(module, ast);
    }

    fclose(file);
    status = eval_module(module, perr) ? 0 : 3;

    // this is dumb
    minim_symbol_table_merge(env->table, module->import->table);
    env->sym_count += module->import->table->size;

    env->current_dir = prev_dir;
    env->module = prev;
    return status;
}

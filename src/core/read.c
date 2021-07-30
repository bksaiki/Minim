#include "../minim.h"

static MinimEnv *get_builtin_env(MinimEnv *env)
{
    if (!env->parent)
        return env;
    
    return get_builtin_env(env->parent);
}

int minim_load_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    ReadTable rt;
    Buffer *mfname, *dir;
    MinimModule *module;
    MinimObject *obj;
    FILE *file;

    init_buffer(&mfname);
    valid_path(mfname, fname);
    file = fopen(mfname->data, "r");

    if (!file)
    {
        printf("Could not open file \"%s\"\n", mfname->data);
        return 2;
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
    module->env->prev = env;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        SyntaxNode *ast, *err;

        minim_parse_port(file, fname, &ast, &err, &rt);
        if (!ast || rt.flags & READ_TABLE_FLAG_BAD)
        {
            printf("; bad syntax: %s", err->sym);
            printf("\n;  in: %s:%zu:%zu\n", fname, rt.row, rt.col);
            fclose(file);
            return 1;
        }

        minim_module_add_expr(module, ast);
    }

    // Evaluate
    eval_module(module->env, module, &obj);
    if (!MINIM_OBJ_VOIDP(obj))
    {
        PrintParams pp;

        set_default_print_params(&pp);
        print_minim_object(obj, env, &pp);
        printf("\n");

        fclose(file);
        return 2;
    }

    fclose(file);
    return 0;
}

#include "../minim.h"

int minim_run_expr(FILE *file, const char *fname, ReadTable *rt, PrintParams *pp, MinimEnv *env)
{
    SyntaxNode *ast, *err;
    MinimObject *obj;

    minim_parse_port(file, fname, &ast, &err, rt);
    if (!ast || rt->flags & READ_TABLE_FLAG_BAD)
    {
        printf("; bad syntax: %s", err->sym);
        printf("\n;  in: %s:%zu:%zu\n", fname, rt->row, rt->col);
        return 1;
    }

    eval_ast(env, ast, &obj);
    if (obj->type == MINIM_OBJ_ERR)
    {    
        print_minim_object(obj, env, pp);
        printf("\n");
        return 2;
    }
    else if (obj->type != MINIM_OBJ_VOID)
    {
        print_minim_object(obj, env, pp);
        printf("\n");
    }

    return 0;
}

int minim_load_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    ReadTable rt;
    Buffer *mfname, *dir;
    FILE *file;
    char *old_dir;

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

    old_dir = env->current_dir;
    dir = get_directory(get_buffer(mfname));
    env->current_dir = get_buffer(dir);

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        if (minim_run_expr(file, mfname->data, &rt, &pp, env))
            break;
    }

    env->current_dir = old_dir;
    fclose(file);
    return 0;
}

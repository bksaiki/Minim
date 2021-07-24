#include <ctype.h>
#include <string.h>
#include "../build/config.h"
#include "minim.h"
#include "read.h"

#define LOAD_FILE(env, file)            \
{                                       \
    if (minim_load_file(env, file))     \
        return 2;                       \
} 

int minim_run_expr(FILE *file, const char *fname, ReadTable *rt, PrintParams *pp, MinimEnv *env)
{
    SyntaxNode *ast, *err;
    MinimObject *obj;

    minim_parse_port(file, fname, &ast, &err, rt);
    if (!ast || rt->flags & READ_TABLE_FLAG_BAD)
    {
        printf("; bad syntax: %s", err->sym);
        printf("\n;  in: %s:%lu:%lu\n", fname, rt->row, rt->col);
        return 1;
    }

    eval_ast(env, ast, &obj);
    GC_collect();

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
    FILE *file;
    Buffer *valid_fname;

    init_buffer(&valid_fname);
    valid_path(valid_fname, fname);
    file = fopen(valid_fname->data, "r");

    if (!file)
    {
        printf("Could not open file \"%s\"\n", valid_fname->data);
        return 2;
    }

    rt.idx = 0;
    rt.row = 1;
    rt.col = 0;
    rt.flags = 0x0;
    rt.eof = EOF;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        if (minim_run_expr(file, valid_fname->data, &rt, &pp, env))
            break;
    }

    fclose(file);
    return 0;
}

int minim_run_file(const char *str, uint32_t flags)
{
    MinimEnv *env;

    init_env(&env, NULL, NULL);
    minim_load_builtins(env);
    if (!(flags & MINIM_FLAG_LOAD_LIBS))
        minim_load_library(env);

    return minim_load_file(env, str);
}

int minim_load_library(MinimEnv *env)
{
    LOAD_FILE(env, MINIM_LIB_PATH "lib/function.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/math.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/list.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/lazy.min");
    
    return 0;
}

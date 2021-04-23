#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "build/config.h"
#include "minim.h"
#include "read.h"

#define LOAD_FILE(env, file)            \
{                                       \
    if (minim_load_file(env, file))     \
        return 2;                       \
} 

int minim_load_file(MinimEnv *env, const char *fname)
{
    SyntaxNode *ast;
    MinimObject *obj;
    PrintParams pp;
    Buffer *valid_fname;
    ReadTable rt;
    FILE *file;

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
    while (~rt.flags & SYNTAX_NODE_FLAG_EOF)
    {
        minim_parse_port2(file, valid_fname->data, &ast, &rt);
        if (!ast || rt.flags & SYNTAX_NODE_FLAG_BAD)
        {
            printf("Parsing failed\n");
            printf("Parsed: %lu at (%lu, %lu)\n", rt.idx, rt.row, rt.col);
            return 2;
        }

        eval_ast(env, ast, &obj);
        if (obj->type == MINIM_OBJ_ERR)
        {    
            print_minim_object(obj, env, &pp);
            printf("\n");

            free_minim_object(obj);
            free_syntax_node(ast);
            return 2;
        }
        else if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj, env, &pp);
            printf("\n");
        }

        free_minim_object(obj);
        free_syntax_node(ast);
    }

    free_buffer(valid_fname);
    fclose(file);
    return 0;
}

int minim_run_file(const char *str, uint32_t flags)
{
    MinimEnv *env;
    int ret;

    init_env(&env, NULL);
    minim_load_builtins(env);
    if (!(flags & MINIM_FLAG_LOAD_LIBS))
        minim_load_library(env);

    ret = minim_load_file(env, str);
    free_env(env);

    return ret;
}

int minim_load_library(MinimEnv *env)
{
    LOAD_FILE(env, MINIM_LIB_PATH "lib/function.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/math.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/list.min");
    
    return 0;
}
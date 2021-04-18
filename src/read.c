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

int run_expr(Buffer *bf, MinimEnv *env, PrintParams *pp, SyntaxLoc *loc)
{
    SyntaxNode *ast;
    MinimObject *obj;
    char *input;
    
    if (bf->pos == 0)
        return 0;
    
    input = get_buffer(bf);
    if (strcmp(input, "(exit)") == 0)
        return 1;
    
    if (!parse_expr_loc(input, &ast, loc))
    {
        printf(";  in: %s:%lu:%lu\n", loc->name, loc->row, loc->col);
        return 2;
    }
    
    eval_ast(env, ast, &obj);
    if (obj->type == MINIM_OBJ_ERR)
    {    
        print_minim_object(obj, env, pp);
        free_minim_object(obj);
        free_syntax_node(ast);
        printf("\n;  in: %s:%lu:%lu\n", loc->name, loc->row, loc->col);
        return 2;
    }
    else if (obj->type != MINIM_OBJ_VOID)
    {
        print_minim_object(obj, env, pp);
        printf("\n");
    }

    free_minim_object(obj);
    free_syntax_node(ast);

    return 0;
}

int minim_load_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    SyntaxLoc *loc, *tloc;
    Buffer *bf;
    Buffer *valid_fname;
    FILE *file;
    int status;

    init_buffer(&valid_fname);
    valid_path(valid_fname, fname);
    file = fopen(valid_fname->data, "r");

    if (!file)
    {
        printf("Could not open file \"%s\"\n", valid_fname->data);
        return 2;
    }

    init_buffer(&bf);
    init_syntax_loc(&loc, valid_fname->data);
    copy_syntax_loc(&tloc, loc);
    set_default_print_params(&pp);

    free_buffer(bf);
    free_buffer(valid_fname);
    free_syntax_loc(loc);
    free_syntax_loc(tloc);
    fclose(file);
    return status;
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "build/config.h"
#include "common/read.h"
#include "minim.h"
#include "read.h"

#define LOAD_FILE(env, file)            \
{                                       \
    if (minim_load_file(env, file))    \
        return 2;                       \
} 

int run_expr(Buffer *bf, MinimEnv *env, PrintParams *pp, SyntaxLoc *loc, uint8_t flags)
{
    MinimAst *ast;
    MinimObject *obj;
    char *input = get_buffer(bf);

    if (strcmp(input, "(exit)") == 0)
        return 1;
    
    if (!parse_str(input, &ast))
    {
        printf("  in: %s:%lu:%lu\n", loc->name, loc->row, loc->col);
        return 2;
    }
    
    eval_ast(env, ast, &obj);
    if (flags & F_READ_START)
    {
        print_minim_object(obj, env, pp);
        printf("\n");
    }
    else if (obj->type == MINIM_OBJ_ERR)
    {
        print_minim_object(obj, env, pp);
        free_minim_object(obj);
        printf("\n");
        printf("  in: %s:%lu:%lu\n", loc->name, loc->row, loc->col);
        return 2;
    }

    free_minim_object(obj);
    free_ast(ast);

    return 0;
}

int minim_load_file(MinimEnv *env, const char *str)
{
    PrintParams pp;
    ReadResult rr;
    SyntaxLoc *loc;
    Buffer *bf;
    FILE *file;
    int status;

    file = fopen(str, "r");
    if (!file)
    {
        printf("Could not open file \"%s\"\n", str);
        return 2;
    }

    init_buffer(&bf);
    init_syntax_loc(&loc, str);
    set_default_print_params(&pp);
    set_default_read_result(&rr);
    
    while (!(rr.flags & READ_RESULT_EOF))
    {
        fread_expr(file, bf, loc, &rr);
        status = run_expr(bf, env, &pp, loc, rr.flags);
        if (status > 0)
        {
            free_buffer(bf);
            fclose(file);
            free(loc);
            return ((status == 1) ? 0 : status);
        }
        else
        {
            reset_buffer(bf);
            rr.flags |= F_READ_START;
            rr.read = 0;
            rr.paren = 0;
        }
    }

    free_buffer(bf);
    fclose(file);
    free(loc);
    return 0;
}

int minim_run_file(const char *str)
{
    MinimEnv *env;
    int ret;

    init_env(&env, NULL);
    minim_load_builtins(env);
    minim_load_library(env);
    ret = minim_load_file(env, str);
    free_env(env);

    return ret;
}

int minim_load_library(MinimEnv *env)
{
    LOAD_FILE(env, MINIM_LIB_PATH "lib/function.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/math.min");
    
    return 0;
}
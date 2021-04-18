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
        free_ast(ast);
        printf("\n;  in: %s:%lu:%lu\n", loc->name, loc->row, loc->col);
        return 2;
    }
    else if (obj->type != MINIM_OBJ_VOID)
    {
        print_minim_object(obj, env, pp);
        printf("\n");
    }

    free_minim_object(obj);
    free_ast(ast);

    return 0;
}

int minim_load_file(MinimEnv *env, const char *fname)
{
    PrintParams pp;
    ReadResult rr;
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
    set_default_read_result(&rr);
    
    while (!(rr.status & READ_RESULT_EOF))
    {
        fread_expr(file, bf, tloc, loc, &rr, EOF);
        if (rr.flags & F_READ_START)
        {
            // inline reset
            bf->pos = 0;
            bf->data[0] = '\0';

            rr.flags |= F_READ_START;
            rr.read = 0;
            rr.paren = 0;
            rr.status &= READ_RESULT_EOF;
        }
        else if (bf->pos > 0)
        {
            status = run_expr(bf, env, &pp, tloc);
            if (status > 0)
            {
                if (status == 1)  status = 0;
                break;
            }
            else
            {
                // inline reset
                bf->pos = 0;
                bf->data[0] = '\0';

                rr.flags |= F_READ_START;
                rr.read = 0;
                rr.paren = 0;
                rr.status &= READ_RESULT_EOF;
            }
        }

        /* Update previous syntax location */
        tloc->row = loc->row;
        tloc->col = loc->col;
    }

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
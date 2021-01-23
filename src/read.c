#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "minim.h"
#include "read.h"

#define F_READ_START         ((uint8_t) 0x1)
#define F_READ_STRING        ((uint8_t) 0x2)
#define F_READ_COMMENT       ((uint8_t) 0x4)
#define F_READ_ESCAPE        ((uint8_t) 0x8)
#define F_READ_SPACE         ((uint8_t) 0x10)

#define LOAD_FILE(env, file)            \
{                                       \
    if (!minim_load_file(env, file))    \
        return 2;                       \
} 

int run_expr(Buffer *bf, MinimEnv *env, PrintParams *pp, uint8_t flags)
{
    MinimAst *ast;
    MinimObject *obj;
    char *input = get_buffer(bf);

    if (strcmp(input, "(exit)") == 0)
    {
        free_buffer(bf);
        return 1;
    }
    
    if (!parse_str(input, &ast))
    {
        free_buffer(bf);
        free_env(env);
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
        printf("\n");

        free_minim_object(obj);
        free_buffer(bf);
        free_env(env);

        return 2;
    }

    free_minim_object(obj);
    free_ast(ast);
    reset_buffer(bf);

    return 0;
}

int minim_load_file(MinimEnv *env, const char *str)
{
    PrintParams pp;
    Buffer *bf;
    FILE *file;
    int paren, status;
    uint8_t flags;

    file = fopen(str, "r");
    if (!file)
    {
        printf("Could not open file \"%s\"\n", str);
        return 2;
    }

    paren = 0, status = 0;
    flags = F_READ_START;
    init_buffer(&bf);
    set_default_print_params(&pp);

    while (1)
    {
        int c = fgetc(file);
        uint8_t nflags = flags;
        
        if (c == EOF)
        {
            if (bf->pos != 0)   run_expr(bf, env, &pp, flags);
            break;
        }
        else if (flags & F_READ_COMMENT)
        {
            if (c == '\n')   nflags &= ~F_READ_COMMENT;
            continue;
        }
        else if (flags & F_READ_STRING)
        {
            if (!(flags & F_READ_ESCAPE) && c == '"')
                nflags &= ~F_READ_STRING;
        }
        else
        {
            if (c == '(')
            {
                ++paren;
                nflags &= ~F_READ_START;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == ')')
            {
                --paren;
                nflags &= ~F_READ_START;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == '"')
            {
                nflags |= F_READ_STRING;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == ';')
            {
                nflags |= F_READ_COMMENT;
                nflags &= ~F_READ_SPACE;
                continue;
            }
            else if (isspace(c))
            {
                nflags |= F_READ_SPACE;
                if (!(flags & F_READ_SPACE) && paren > 0)   c = ' ';
                else                                        continue;
            }
            else
            {
                nflags &= ~F_READ_SPACE;
            }
        }

        if (c == '\\')   nflags |= F_READ_ESCAPE;
        else             nflags &= ~F_READ_ESCAPE;

        writec_buffer(bf, c);
        if ((!(flags & F_READ_START) || (nflags & F_READ_SPACE)) && paren == 0)
        {
            status = run_expr(bf, env, &pp, flags);
            nflags |= F_READ_START;
            paren = 0;

            if (status > 0)
            {
                if (status == 1)    status = 0;
                break;
            }
        }

        flags = nflags;
    }

    free_buffer(bf);
    fclose(file);

    return status;
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
    LOAD_FILE(env, "src/lib/function.min");
    
    return 0;
}
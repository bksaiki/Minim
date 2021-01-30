#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "build/config.h"
#include "minim.h"
#include "read.h"

#define F_READ_START         ((uint8_t) 0x1)
#define F_READ_STRING        ((uint8_t) 0x2)
#define F_READ_COMMENT       ((uint8_t) 0x4)
#define F_READ_ESCAPE        ((uint8_t) 0x8)
#define F_READ_SPACE         ((uint8_t) 0x10)

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
    SyntaxLoc loc;
    Buffer *bf;
    FILE *file;
    size_t nrow, ncol;
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

    loc.name = malloc((strlen(str) + 1) * sizeof(char));
    strcpy(loc.name, str);
    loc.row = 1; loc.col = 0;
    nrow = 1; ncol = 0;

    while (1)
    {
        int c = fgetc(file);
        uint8_t nflags = flags;
        
        // Check for EOF
        if (c == EOF)
        {
            if (bf->pos != 0)   run_expr(bf, env, &pp, &loc, flags);
            break;
        }

        // Update location
        if (c == '\n')
        {
            ++nrow;
            ncol = 0;
        }
        else
        {
            ++ncol;
        }

        // Handle character
        if (flags & F_READ_COMMENT)
        {
            if (c == '\n')
            {
                flags &= ~F_READ_COMMENT;
                loc.row = nrow;
                loc.col = ncol;
            }
            
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
                flags |= F_READ_COMMENT;
                flags &= ~F_READ_SPACE;
                continue;
            }
            else if (isspace(c))
            {
                if (flags & F_READ_START)
                {
                    continue;
                }
                else if (!(flags & F_READ_SPACE) && paren > 0)
                {
                    c = ' ';
                    nflags |= F_READ_SPACE;
                }
                else
                {
                    flags |= F_READ_SPACE;
                    continue;
                }
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
            status = run_expr(bf, env, &pp, &loc, flags);
            nflags |= F_READ_START;
            loc.row = nrow;
            loc.col = ncol;
            paren = 0;

            if (status > 0)
            {
                free_buffer(bf);
                if (status == 1)    status = 0;
                break;
            }
            else
            {
                reset_buffer(bf);
            }
        }

        flags = nflags;
    }

    free_buffer(bf);
    fclose(file);
    free(loc.name);

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
    LOAD_FILE(env, MINIM_LIB_PATH "lib/function.min");
    LOAD_FILE(env, MINIM_LIB_PATH "lib/math.min");
    
    return 0;
}
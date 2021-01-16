#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "minim.h"
#include "read.h"

int minim_run_file(const char *str)
{
    MinimEnv *env;
    PrintParams pp;
    Buffer *bf;

    FILE *file;
    int paren;

    file = fopen(str, "r");
    if (!file)
    {
        printf("Could not open file \"%s\"\n", str);
        return 2;
    }

    paren = 0;
    init_buffer(&bf);

    init_env(&env, NULL);
    env_load_builtins(env);
    set_default_print_params(&pp);

    while (1)
    {
        int c = fgetc(file);

        if (c == EOF)           break;
        else if (c == '(')      ++paren;
        else if (c == ')')      --paren;
        else if (isspace(c))
        {
            if (paren > 0)  c = ' ';
            else            continue;
        }

        writec_buffer(bf, c);
        if (paren == 0)
        {
            MinimAst *ast;
            MinimObject *obj;
            char *input = get_buffer(bf);

            if (strcmp(input, "(exit)") == 0)
            {
                free_buffer(bf);
                break;
            }
            
            if (!parse_str(input, &ast))
            {
                free_buffer(bf);
                free_env(env);
                return 2;
            }
            
            eval_ast(env, ast, &obj);
            if (obj->type == MINIM_OBJ_ERR)
            {
                print_minim_object(obj, env, &pp);
                printf("\n");

                free_minim_object(obj);
                free_buffer(bf);
                free_env(env);

                return 2;
            }

            free_minim_object(obj);
            free_ast(ast);

            reset_buffer(bf);
            paren = 0;
        }
    }

    free_buffer(bf);
    free_env(env);
    fclose(file);

    return 0;
}
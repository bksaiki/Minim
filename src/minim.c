#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "minim.h"

int main(int argc, char** argv)
{
    MinimEnv *env;
    MinimAst *ast;
    MinimObject *obj;
    Buffer *bf;

    init_env(&env);
    env_load_builtins(env);

    printf("Minim v%s\n", MINIM_VERSION_STR);
    while (1)
    {
        char *input;
        int paren = 0;

        init_buffer(&bf);
        fputs("> ", stdout);

        while (1)
        {
            char str[2048];
            int len;

            fgets(str, 2047, stdin);
            for (len = 0; str[len] != '\n'; ++len)
            {
                if (str[len] == '(')          ++paren;
                else if (str[len] == ')')     --paren;
            }

            if (bf->pos != 0)   writec_buffer(bf, ' ');
            write_buffer(bf, str, len);

            if (paren <= 0)     break;

            fputs("  ", stdout);
            for (int i = 0; i < paren; ++i)
                fputs("  ", stdout);
        }

        input = get_buffer(bf);
        if (strlen(input) == 0)
        {
            free_buffer(bf);
            continue;
        }
        else if (strcmp(input, "(exit)") == 0)
        {
            free_buffer(bf);
            break;
        }

        if (!parse_str(input, &ast))
        {
            free_buffer(bf);
            continue;
        }
        
        eval_ast(env, ast, &obj);
        if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj, env, INT_MAX);
            printf("\n");
        }

        free_minim_object(obj);
        free_ast(ast);
        free_buffer(bf);
    }

    free_env(env);
    return 0;
}
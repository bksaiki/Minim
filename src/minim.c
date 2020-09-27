#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "minim.h"

int main(int argc, char** argv)
{
    MinimEnv *env;
    MinimAstNode *ast;
    MinimObject *obj;
    char *input;
    int ilen = 2048;

    init_env(&env);
    env_load_builtins(env);

    printf("Minim v%s\n", MINIM_VERSION_STR);
    while (1)
    {
        char str[2048];
        int paren = 0;

        input = malloc(ilen * sizeof(char));
        fputs("> ", stdout);
        strcpy(input, "");

        while (1)
        {
            int len = 0;

            fgets(str, 2047, stdin);
            for (; str[len] != '\n'; ++len)
            {
                if (str[len] == '(')          ++paren;
                else if (str[len] == ')')     --paren;
            }

            if (strlen(input) + len >= ilen)
            {
                input = realloc(input, ilen * 2);
                ilen *= 2;
            }

            if (strlen(input) != 0)
                strcat(input, " ");

            strncat(input, str, len);
            if (!paren) break;

            fputs("  ", stdout);
            for (int i = 0; i < paren; ++i)
                fputs("  ", stdout);
        }

        if (strlen(input) == 0)
        {
            free(input);
            continue;
        }
        else if (strcmp(input, "(exit)") == 0)
        {
            free(input);
            break;
        }

        if (!parse_str(input, &ast))
        {
            free(input);
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
        free(input);
    }

    free_env(env);
    return 0;
}
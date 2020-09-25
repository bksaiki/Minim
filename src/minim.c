#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "eval.h"
#include "parser.h"

int main(int argc, char** argv)
{
    MinimEnv *env;
    MinimAstNode *ast;
    MinimObject *obj;
    char input[2048];

    printf("Minim v%s\n", MINIM_VERSION_STR);
    init_env(&env);
    env_load_builtins(env);

    while (1)
    {
        char *str;
        int len = 0;

        fputs("> ", stdout);
        fgets(input, 2047, stdin);
        
        for (; input[len] != '\n'; ++len);
        str = calloc(len + 1, sizeof(char));
        strncpy(str, input, len);

        if (strlen(str) == 0)
        {
            free(str);
            continue;
        }
        else if (strcmp(str, "(exit)") == 0)
        {
            free(str);
            break;
        }

        if (!parse_str(str, &ast))
        {
            fputs("Parsing failed", stdout);
            free(str);
            continue;
        }

        eval_ast(env, ast, &obj);
        if (obj->type != MINIM_OBJ_VOID)
        {
            print_minim_object(obj);
            printf("\n");
        }

        free_minim_object(obj);
        free_ast(ast);
        free(str);
    }

    free_env(env);

    return 0;
}
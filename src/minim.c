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

    char input[1024];
    char* str;
    int idx;

    printf("Minim v%s\n", MINIM_VERSION_STR);
    init_env(&env);
    env_load_builtins(env);

    while (1)
    {
        printf("> ");
        fgets(input, 1024, stdin);

        for (idx = 0; input[idx] != '\0' && input[idx] != '\n'; ++idx);
        str = calloc(sizeof(char), idx + 1);
        strncpy(str, input, idx);

        if (strcmp(str, "(exit)") == 0)
        {
            free(str);
            break;
        }

        if (!parse_str(str, &ast))
        {
            free(str);
            continue;
        }

        eval_ast(env, ast, &obj);
        print_minim_object(obj);
        printf("\n");

        free_minim_object(obj);
        free_ast(ast);
        free(str);
    }

    free_env(env);

    return 0;
}
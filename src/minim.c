#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"
#include "parser.h"

int main(int argc, char** argv)
{
    MinimAstWrapper ast;
    MinimObjectWrapper objw;
    char input[1024];
    char* str;
    int idx;

    printf("Minim v%s\n", MINIM_VERSION_STR);

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

        eval_ast(&ast, &objw);
        print_minim_object(objw.obj);
        printf("\n");

        free_minim_object(objw.obj);
        free_ast(ast.node);
        free(str);
    }

    return 0;
}
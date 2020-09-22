#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

int main(int argc, char** argv)
{
    char input[1024];
    char* str;
    int idx;

    printf("Minim v%s\n", MINIM_VERSION_STR);

    while (1)
    {
        printf("> ");
        fgets(input, 1024, stdin);

        for (idx = 0; input[idx] != '\0' && input[idx] != '\n'; ++idx);
        str = malloc((idx + 1) * sizeof(char));
        strncpy(str, input, idx);

        MinimAstWrapper ast;

        if (!parse_str(str, &ast))
            continue;

        print_ast(ast.node);
        printf("\n");

        free_ast(ast.node);
    }

    return 0;
}
#include <stdio.h>
#include "parser.h"

int main(int argc, char** argv)
{
    char str[100];

    printf("Minim v%s\n", MINIM_VERSION_STR);
    printf("> ");
    scanf("%s", str);
    return 0;
}
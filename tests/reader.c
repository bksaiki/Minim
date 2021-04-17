#include <stdio.h>
#include <stdlib.h>
#include "../src/proto/reader.h"

int main(int argc, char **argv)
{
    ReadNode *node;

    printf("> ");
    node = minim_read_str(stdin);
    if (node) print_syntax(node);
    printf("\n");
}
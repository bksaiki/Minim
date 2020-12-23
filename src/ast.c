#include <stdlib.h>
#include <string.h>
#include "ast.h"

void init_ast_op(MinimAst **past, int argc, int tags)
{
    MinimAst *node = malloc(sizeof(MinimAst));
    node->children = malloc(argc * sizeof(MinimAst*));
    node->argc = argc;
    node->sym = NULL;
    node->tags = MINIM_AST_OP | tags;
    *past = node;
}

void init_ast_node(MinimAst **past, const char *sym, int tags)
{
    MinimAst *node = malloc(sizeof(MinimAst));
    node->children = NULL;
    node->argc = 0;
    node->sym = malloc((strlen(sym) + 1) * sizeof(char));
    node->tags = MINIM_AST_VAL | tags;
    *past = node;

    strcpy(node->sym, sym);
}

void copy_ast(MinimAst **pdest, MinimAst *src)
{
    MinimAst *node = malloc(sizeof(MinimAst));
    node->argc = src->argc;
    node->tags = src->tags;
    *pdest = node;

    if (src->sym)
    {
        node->sym = malloc((strlen(src->sym) + 1) * sizeof(char));
        strcpy(node->sym, src->sym);
    }
    else
    {
        node->sym = NULL;
    }

    if (src->children)
    {
        node->children = malloc(src->argc * sizeof(MinimAst*));
        for (int i = 0; i < src->argc; ++i)
            copy_ast(&node->children[i], src->children[i]);
    }
    else
    {
        node->children = NULL;
    }
}

void free_ast(MinimAst* node)
{
    if (!node)  return; // abort early

    if (node->argc != 0)
    {
        for (int i = 0; i < node->argc; ++i)
            free_ast(node->children[i]);
    }

    if (node->sym)      free(node->sym);
    if (node->children) free(node->children);
    free(node);
}

bool valid_ast(MinimAst *node)
{
    if (node->tags & MINIM_AST_ERR)
        return false;

    for (int i = 0; i < node->argc; ++i)
    {
        if (!valid_ast(node->children[i]))
            return false;
    }

    return true;
}
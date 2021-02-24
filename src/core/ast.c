#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

void init_ast_op(MinimAst **past, size_t argc, uint8_t tags)
{
    MinimAst *node = malloc(sizeof(MinimAst));
    node->children = malloc(argc * sizeof(MinimAst*));
    node->loc = NULL;
    node->argc = argc;
    node->sym = NULL;
    node->tags = MINIM_AST_OP | tags;
    *past = node;
}

void init_ast_node(MinimAst **past, const char *sym, int tags)
{
    MinimAst *node = malloc(sizeof(MinimAst));
    node->children = NULL;
    node->loc = NULL;
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

    if (src->loc)   copy_syntax_loc(&node->loc, src->loc);
    else            node->loc = NULL;

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
        for (size_t i = 0; i < src->argc; ++i)
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
        for (size_t i = 0; i < node->argc; ++i)
            free_ast(node->children[i]);
    }

    if (node->loc)      free_syntax_loc(node->loc);
    if (node->sym)      free(node->sym);
    if (node->children) free(node->children);
    free(node);
}

void ast_add_syntax_loc(MinimAst *ast, SyntaxLoc *loc)
{
    if (ast->loc)   free_syntax_loc(ast->loc);
    copy_syntax_loc(&ast->loc, loc);
}

bool ast_validp(MinimAst *node)
{
    if (node->tags & MINIM_AST_ERR)
        return false;

    for (size_t i = 0; i < node->argc; ++i)
    {
        if (!ast_validp(node->children[i]))
            return false;
    }

    return true;
}

bool ast_equalp(MinimAst *a, MinimAst *b)
{
    if (a->argc != b->argc || a->tags != b->tags)
        return false;
    
    if ((!a->sym && b->sym) || (a->sym && !b->sym) ||
        (a->sym && b->sym && strcmp(a->sym, b->sym) != 0))
        return false;

    for (size_t i = 0; i < a->argc; ++i)
    {
        if (!ast_equalp(a->children[i], b->children[i]))
            return false;
    }

    return true;
}

void ast_to_buffer(MinimAst *node, Buffer *bf)
{
    if (node->argc != 0)
    {
        bool first = true;

        writec_buffer(bf, '(');
        for (size_t i = 0; i < node->argc; ++i)
        {
            if (first)  first = false;
            else        writec_buffer(bf, ' ');
            ast_to_buffer(node->children[i], bf);
        }

        writec_buffer(bf, ')');
    }
    else
    {
        writes_buffer(bf, node->sym);
    }
}

void ast_dump_in_buffer(MinimAst *node, Buffer *bf)
{
    if (node->argc != 0)
    {
        for (size_t i = 0; i < node->argc; ++i)
            ast_dump_in_buffer(node->children[i], bf);
    }
    else
    {
        writes_buffer(bf, node->sym);
    }
}

void print_ast(MinimAst *node)  // debugging
{
    Buffer *bf;

    init_buffer(&bf);
    ast_to_buffer(node, bf);
    fputs(bf->data, stdout);
    free_buffer(bf);
}
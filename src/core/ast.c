#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
#include "ast.h"

void init_syntax_loc(SyntaxLoc **ploc, const char *fname)
{
    SyntaxLoc *loc = GC_alloc(sizeof(SyntaxLoc));
    loc->name = GC_alloc((strlen(fname) + 1) * sizeof(char));
    loc->row = 1;
    loc->col = 1;
    *ploc = loc;

    strcpy(loc->name, fname);
}

void copy_syntax_loc(SyntaxLoc **ploc, SyntaxLoc *src)
{
    SyntaxLoc *loc = GC_alloc(sizeof(SyntaxLoc));
    loc->name = GC_alloc((strlen(src->name) + 1) * sizeof(char));
    loc->row = src->row;
    loc->col = src->col;
    *ploc = loc;

    strcpy(loc->name, src->name);
}

void free_syntax_loc(SyntaxLoc *loc)
{
    /* Nothing */
}

void init_syntax_node(SyntaxNode **pnode, SyntaxNodeType type)
{
    SyntaxNode *node = GC_alloc(sizeof(SyntaxNode));

    node->children = NULL;
    node->childc = 0;
    node->sym = NULL;
    node->loc = NULL;
    node->type = type;
    *pnode = node;
}

void copy_syntax_node(SyntaxNode **pnode, SyntaxNode *src)
{
    SyntaxNode *node = GC_alloc(sizeof(SyntaxNode));

    node->type = src->type;
    node->childc = src->childc;
    *pnode = node;

    if (node->childc > 0)
    {
        node->children = GC_alloc(node->childc * sizeof(SyntaxNode*));
        for (size_t i = 0; i < node->childc; ++i)
            copy_syntax_node(&node->children[i], src->children[i]);
    }
    else
    {
        node->children = NULL;
    }
    
    if (src->sym)
    {
        node->sym = GC_alloc((strlen(src->sym) + 1) * sizeof(char));
        strcpy(node->sym, src->sym);
    }
    else
    {
        node->sym = NULL;
    }

    if (src->loc)   copy_syntax_loc(&node->loc, src->loc);
    else            node->loc = NULL;
}

void free_syntax_node(SyntaxNode *node)
{
    /* Nothing */
}

void print_syntax(SyntaxNode *node)
{
    if (node->childc > 0)
    {
        printf("(");
        print_syntax(node->children[0]);
        for (size_t i = 1; i < node->childc; ++i)
        {
            printf(" ");
            print_syntax(node->children[i]);
        }
        printf(")");
    }
    else
    {
        printf("%s", node->sym);
    }
}

void ast_add_syntax_loc(SyntaxNode *ast, SyntaxLoc *loc)
{
    if (ast->loc)   free_syntax_loc(ast->loc);
    ast->loc = loc;
}

bool ast_validp(SyntaxNode *node)
{
    return true;
}

bool ast_equalp(SyntaxNode *a, SyntaxNode *b)
{
    if (a->type != b->type || a->childc != b->childc)
        return false;

    for (size_t i = 0; i < a->childc; ++i)
    {
        if (!ast_equalp(a->children[i], b->children[i]))
            return false;
    }

    return strcmp(a->sym, b->sym) == 0;
}

void ast_to_buffer(SyntaxNode *node, Buffer *bf)
{
    if (node->childc != 0)
    {
        writec_buffer(bf, '(');
        ast_to_buffer(node->children[0], bf);
        for (size_t i = 1; i < node->childc; ++i)
        {
            writec_buffer(bf, ' ');
            ast_to_buffer(node->children[i], bf);
        }

        writec_buffer(bf, ')');
    }
    else
    {
        writes_buffer(bf, node->sym);
    }
}

void ast_dump_in_buffer(SyntaxNode *node, Buffer *bf)
{
    if (node->childc != 0)
    {
        for (size_t i = 0; i < node->childc; ++i)
            ast_dump_in_buffer(node->children[i], bf);
    }
    else
    {
        writes_buffer(bf, node->sym);
    }
}

void print_ast(SyntaxNode *node)  // debugging
{
    Buffer *bf;

    init_buffer(&bf);
    ast_to_buffer(node, bf);
    fputs(bf->data, stdout);
    free_buffer(bf);
}

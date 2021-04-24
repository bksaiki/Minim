#ifndef _MINIM_AST_H_
#define _MINIM_AST_H_

#include "../common/buffer.h"
#include "../common/common.h"

struct SyntaxLoc
{
    char *name;
    size_t row;
    size_t col;
} typedef SyntaxLoc;

void init_syntax_loc(SyntaxLoc **ploc, const char *fname);
void copy_syntax_loc(SyntaxLoc **ploc, SyntaxLoc *src);
void free_syntax_loc(SyntaxLoc *loc);

enum SyntaxNodeType
{
    SYNTAX_NODE_DATUM,
    SYNTAX_NODE_LIST,
    SYNTAX_NODE_PAIR,
    SYNTAX_NODE_VECTOR
} typedef SyntaxNodeType;

struct SyntaxNode
{
    struct SyntaxNode **children;
    SyntaxLoc *loc;
    size_t childc;
    char *sym;
    SyntaxNodeType type;
} typedef SyntaxNode;

void init_syntax_node(SyntaxNode **pnode, SyntaxNodeType type);
void copy_syntax_node(SyntaxNode **pnode, SyntaxNode *src);
void free_syntax_node(SyntaxNode *node);
void print_syntax(SyntaxNode *node);

void ast_add_syntax_loc(SyntaxNode *ast, SyntaxLoc *loc);
bool ast_validp(SyntaxNode *node);
bool ast_equalp(SyntaxNode *a, SyntaxNode *b);

void ast_to_buffer(SyntaxNode *node, Buffer *bf);
void ast_dump_in_buffer(SyntaxNode *node, Buffer *bf);

void print_ast(SyntaxNode *node);  // debugging

#endif

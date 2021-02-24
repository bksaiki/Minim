#ifndef _MINIM_AST_H_
#define _MINIM_AST_H_

#include "../common/buffer.h"
#include "../common/common.h"
#include "../common/read.h"

#define MINIM_AST_OP    0x1
#define MINIM_AST_VAL   0x2
#define MINIM_AST_ERR   0x80

typedef struct MinimAst
{
    struct MinimAst** children;
    SyntaxLoc *loc;
    char* sym;
    size_t argc;
    uint8_t tags;
} MinimAst;

void init_ast_op(MinimAst **past, size_t argc, uint8_t tags);
void init_ast_node(MinimAst **past, const char *sym, int tags);
void copy_ast(MinimAst **pdest, MinimAst *src);
void free_ast(MinimAst *node);

void ast_add_syntax_loc(MinimAst *ast, SyntaxLoc *loc);
bool ast_validp(MinimAst *node);
bool ast_equalp(MinimAst *a, MinimAst *b);

void ast_to_buffer(MinimAst *node, Buffer *bf);
void ast_dump_in_buffer(MinimAst *node, Buffer *bf);

void print_ast(MinimAst *node);  // debugging

#endif
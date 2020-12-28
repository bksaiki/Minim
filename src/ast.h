#ifndef _MINIM_AST_H_
#define _MINIM_AST_H_

#include "common/buffer.h"
#include "common/common.h"

#define MINIM_AST_OP    0x1
#define MINIM_AST_VAL   0x2
#define MINIM_AST_ERR   0x80

typedef struct MinimAst
{
    struct MinimAst** children;
    char* sym;
    int argc;
    int tags;
} MinimAst;

void init_ast_op(MinimAst **past, int argc, int tags);
void init_ast_node(MinimAst **past, const char *sym, int tags);
void copy_ast(MinimAst **pdest, MinimAst *src);
void free_ast(MinimAst *node);

bool ast_validp(MinimAst *node);
bool ast_equalp(MinimAst *a, MinimAst *b);

void ast_to_buffer(MinimAst *node, Buffer *bf);
void ast_dump_in_buffer(MinimAst *node, Buffer *bf);

void print_ast(MinimAst *node);  // debugging

#endif
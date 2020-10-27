#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "base.h"

typedef enum MinimAstState
{
    MINIM_AST_VALID,
    MINIM_AST_ERROR
} MinimAstState;

typedef enum MinimAstTag
{
    MINIM_AST_VAL,
    MINIM_AST_OP,
    MINIM_AST_NONE
} MinimAstTag;

typedef struct MinimAst
{
    struct MinimAst** children;
    char* sym;
    int count;
    MinimAstState state;
    MinimAstTag tag;
} MinimAst;

// Copies the ast at 'src' and saves it at 'dest'.
void copy_ast(MinimAst **dest, MinimAst *src);

// Deletes an AST
void free_ast(MinimAst* node);

// Parses a single expression. Returns null on failure.
int parse_str(char* str, MinimAst** syn);

// Prints an AST syntax tree
void print_ast(MinimAst* node);


#endif
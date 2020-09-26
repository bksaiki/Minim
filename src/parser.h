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

typedef struct MinimAstNode
{
    struct MinimAstNode** children;
    char* sym;
    int count;
    MinimAstState state;
    MinimAstTag tag;
} MinimAstNode;

// Copies the ast at 'src' and saves it at 'dest'.
void copy_ast(MinimAstNode **dest, MinimAstNode *src);

// Deletes an AST
void free_ast(MinimAstNode* node);

// Parses a single expression. Returns null on failure.
int parse_str(char* str, MinimAstNode** syn);

// Prints an AST syntax tree
void print_ast(MinimAstNode* node);


#endif
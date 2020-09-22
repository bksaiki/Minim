#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "object.h"

typedef enum _MinimAstState
{
    MINIM_AST_VALID,
    MINIM_AST_ERROR
} MinimAstState;

typedef enum _MinimAstTag
{
    MINIM_AST_VAL,
    MINIM_AST_OP,
    MINIM_AST_NONE
} MinimAstTag;

typedef struct _MinimAstNode
{
    struct _MinimAstNode** children;
    char* sym;
    int argc;
    MinimAstState state;
    MinimAstTag tag;
} MinimAstNode;

typedef struct _MinimAstWrapper
{
    MinimAstNode* node;
} MinimAstWrapper;

// Deletes an AST
void free_ast(MinimAstNode* node);

// Parses a single expression. Returns null on failure.
int parse_str(const char* str, MinimAstWrapper* syn);

// Prints an AST syntax tree
void print_ast(MinimAstNode* node);


#endif
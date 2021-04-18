#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "../common/common.h"
#include "../common/read.h"
#include "ast.h"

struct ReadTable
{
    size_t idx, row, col;
    uint8_t flags;
    char eof;
} typedef ReadTable;

int minim_parse_port(FILE *file, const char *name, SyntaxNode **psyntax,
                     char eof, bool wait);

// Parses a single expression
int parse_str(const char* str, SyntaxNode** psyntax);

// Parses a single expression with syntax location
int parse_expr_loc(const char* str, SyntaxNode** psyntax, SyntaxLoc *loc);

#endif
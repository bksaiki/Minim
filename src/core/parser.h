#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "../common/common.h"
#include "ast.h"


#define READ_TABLE_FLAG_EOF      0x1 // eof encoutered
#define READ_TABLE_FLAG_BAD      0x2 // error while parsing
#define READ_TABLE_FLAG_WAIT     0x4 // behavior on eof

struct ReadTable
{
    size_t idx, row, col;
    uint8_t flags;
    char eof;
} typedef ReadTable;

int minim_parse_port(FILE *file,
                     const char *name,
                     SyntaxNode **psyntax,
                     SyntaxNode **perr,
                     ReadTable *table);

// Parses a single expression
int parse_str(const char* str, SyntaxNode** psyntax);

#endif

#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "ast.h"
#include "object.h"

#define READ_TABLE_FLAG_EOF      0x1 // eof encoutered
#define READ_TABLE_FLAG_BAD      0x2 // error while parsing
#define READ_TABLE_FLAG_WAIT     0x4 // behavior on eof

#define READ_FLAG_WAIT          0x1     // behavior upon encountering end of inpt

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

SyntaxNode *minim_parse_port2(MinimObject *port, SyntaxNode **perr, uint8_t flags);

// Set read table
void set_default_read_table(ReadTable *table);

// Parses a single expression
int parse_str(const char* str, SyntaxNode** psyntax);

#endif

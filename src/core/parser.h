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

SyntaxNode *minim_parse_port(MinimObject *port, SyntaxNode **perr, uint8_t flags);

// Set read table
void set_default_read_table(ReadTable *table);

#endif

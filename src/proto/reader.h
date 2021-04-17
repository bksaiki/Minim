#ifndef _PROTOTYPE_READER_H_
#define _PROTOTYPE_READER_H_

#include <stdbool.h>
#include <stdint.h>

#define READ_NODE_FLAG_EOF   0x1
#define READ_NODE_FLAG_BAD   0x2

enum ReadNodeType
{
    READ_NODE_DATUM,
    READ_NODE_LIST,
    READ_NODE_PAIR,
    READ_NODE_VECTOR
} typedef ReadNodeType;

struct ReadNode
{
    struct ReadNode **children;
    size_t childc;
    char *sym;
    ReadNodeType type;
} typedef ReadNode;

struct SyntaxTable
{
    size_t idx, row, col;
    uint8_t flags;
    char eof;
    char wait;
} typedef SyntaxTable;

void print_syntax(ReadNode *node);

ReadNode *minim_read_str(FILE *file);

#endif
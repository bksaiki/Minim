#ifndef _PROTOTYPE_READER_H_
#define _PROTOTYPE_READER_H_

#include <stdbool.h>
#include <stdint.h>

struct SyntaxTable
{
    size_t idx, row, col;
    uint8_t flags;
    char eof;
} typedef SyntaxTable;

SyntaxNode *minim_read_str(FILE *file, char eof, char wait);

#endif
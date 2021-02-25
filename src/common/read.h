#ifndef _MINIM_READ_H_
#define _MINIM_READ_H_

#include <stdio.h>
#include "common.h"
#include "buffer.h"

#define READ_RESULT_SUCCESS         0x0
#define READ_RESULT_EOF             0x1
#define READ_RESULT_INCOMPLETE      0xF0
#define READ_RESULT_FAILURE         0xFF

#define F_READ_START         ((uint8_t) 0x1)
#define F_READ_STRING        ((uint8_t) 0x2)
#define F_READ_COMMENT       ((uint8_t) 0x4)
#define F_READ_ESCAPE        ((uint8_t) 0x8)
#define F_READ_SPACE         ((uint8_t) 0x10)

struct SyntaxLoc
{
    char *name;
    size_t row;
    size_t col;
} typedef SyntaxLoc;

struct ReadResult
{
    size_t read;
    long int paren;
    uint8_t status;
    uint8_t flags;
} typedef ReadResult;

// *** SyntaxLoc *** //

void init_syntax_loc(SyntaxLoc **ploc, const char *fname);
void copy_syntax_loc(SyntaxLoc **ploc, SyntaxLoc *src);
void free_syntax_loc(SyntaxLoc *loc);

// *** ReadResult *** //

void set_default_read_result(ReadResult *rr);

// *** Reading *** //

void fread_expr(FILE *file, Buffer *bf, SyntaxLoc *loc, ReadResult *rr, char eof);
void valid_path(Buffer *valid, const char *maybe);

#endif
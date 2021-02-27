#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "read.h"

void init_syntax_loc(SyntaxLoc **ploc, const char *fname)
{
    SyntaxLoc *loc = malloc(sizeof(SyntaxLoc));
    loc->name = malloc((strlen(fname) + 1) * sizeof(char));
    loc->row = 1;
    loc->col = 1;
    *ploc = loc;

    strcpy(loc->name, fname);
}

void copy_syntax_loc(SyntaxLoc **ploc, SyntaxLoc *src)
{
    SyntaxLoc *loc = malloc(sizeof(SyntaxLoc));
    loc->name = malloc((strlen(src->name) + 1) * sizeof(char));
    loc->row = src->row;
    loc->col = src->col;
    *ploc = loc;

    strcpy(loc->name, src->name);
}

void free_syntax_loc(SyntaxLoc *loc)
{
    if (loc->name)     free(loc->name);
    free(loc);
}

void set_default_read_result(ReadResult *rr)
{
    rr->read = 0;
    rr->paren = 0;
    rr->status = READ_RESULT_SUCCESS;
    rr->flags = F_READ_START;
}

static int fread_char(FILE *file, ReadResult *rr, SyntaxLoc *sloc, SyntaxLoc *eloc, uint8_t flags, char eof)
{
    int c = fgetc(file);

    ++rr->read;
    if (c == eof)
    {
        rr->status |= READ_RESULT_EOF;
    }
    else if (c == '\n')
    {
        ++eloc->row;
        eloc->col = 1;
        
        if (flags & F_READ_START)
        {
            ++sloc->row;
            sloc->col = 1;
        }
    }
    else
    {
        ++eloc->col;
        if (flags & F_READ_START)
            ++sloc->col;
    }

    return c;
}

void fread_expr(FILE *file, Buffer *bf, SyntaxLoc *sloc, SyntaxLoc *eloc, ReadResult *rr, char eof)
{
    uint8_t flags = rr->flags;

    while (1)
    {
        if (flags & F_READ_STRING)
        {
            while (1)
            {
                int c = fread_char(file, rr, sloc, eloc, flags, eof);

                if (c == eof)   break;

                writec_buffer(bf, c);
                if (c == '\\')  flags |= F_READ_ESCAPE;
                else if (!(flags & F_READ_ESCAPE) && c == '"')
                {
                    flags &= ~F_READ_STRING;
                    break;
                }
            }
        }
        else if (flags & F_READ_COMMENT)
        {
            while (1)
            {
                int c = fread_char(file, rr, sloc, eloc, flags, eof);

                if (c == eof)   break;
                if (c == '\n')
                {
                    flags &= ~F_READ_COMMENT;
                    break;
                }
            }
        }
        else
        {
            int c = fread_char(file, rr, sloc, eloc, flags, eof);

            if (c == eof) break;
            flags &= ~F_READ_ESCAPE;

            if (c == '(')
            {
                writec_buffer(bf, c);
                ++rr->paren;
                flags &= ~F_READ_START;
                flags &= ~F_READ_SPACE;
            }
            else if (c == ')')
            {
                writec_buffer(bf, c);
                --rr->paren;
                flags &= ~F_READ_START;
                flags &= ~F_READ_SPACE;
            }
            else if (c == '"')
            {
                writec_buffer(bf, c);
                flags |= F_READ_STRING;
                flags &= ~F_READ_START;
                flags &= ~F_READ_SPACE;
            }
            else if (c == ';')
            {
                flags |= F_READ_COMMENT;
                flags &= ~F_READ_SPACE;
                continue;
            }
            else if (isspace(c))
            {
                if (c == '\n')
                {
                    if (rr->paren > 0)
                        writec_buffer(bf, ' ');
                }
                else
                {
                    writec_buffer(bf, ' ');
                }

                flags |= F_READ_SPACE;
            }
            else
            {
                writec_buffer(bf, c);
                flags &= ~F_READ_START;
                flags &= ~F_READ_SPACE;
            }

            if (((flags & F_READ_SPACE) || !(flags & F_READ_START)) && rr->paren == 0)
            {
                rr->status = READ_RESULT_SUCCESS;
                rr->flags = flags;
                break;
            }
        }

        // Check for EOF
        if (rr->status & READ_RESULT_EOF || rr->status & READ_RESULT_SUCCESS)
        {
            return;
        }   
    }
}

void valid_path(Buffer *valid, const char *maybe)
{
#ifdef MINIM_WINDOWS
    size_t len = strlen(maybe);
    bool first = (len > 0 && maybe[0] == '/');

    for (size_t i = 0; i < len; ++i)
    {
        if (maybe[i] == '/')
        {
            if (i != 0)
            {
                if (first) 
                {
                    writec_buffer(valid, ':');
                    first = false;
                }
                
                writes_buffer(valid, "\\\\");
            }
        }
        else
        {
            writec_buffer(valid, maybe[i]);
        }
    }
#else
    writes_buffer(valid, maybe);
#endif
}
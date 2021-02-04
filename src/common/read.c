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

void fread_expr(FILE *file, Buffer *bf, SyntaxLoc *loc, ReadResult *rr, char eof)
{
    size_t nrow = loc->row;
    size_t ncol = loc->col;
    uint8_t flags = rr->flags;

    while (1)
    {
        int c = fgetc(file);
        uint8_t nflags = flags;
        
        // Check for EOF
        if (c == eof)
        {
            rr->status |= READ_RESULT_EOF;
            rr->status |= ((rr->paren > 0) ? READ_RESULT_INCOMPLETE : READ_RESULT_SUCCESS);
            rr->flags = flags;
            loc->row = nrow;
            loc->col = ncol;
            break;
        }

        // Update location
        ++rr->read;
        if (c == '\n')
        {
            ++nrow;
            ncol = 1;
        }
        else
        {
            ++ncol;
        }

        // Handle character
        if (flags & F_READ_COMMENT)
        {
            if (c == '\n')
            {
                flags &= ~F_READ_COMMENT;
                loc->row = nrow;
                loc->col = ncol;
                writec_buffer(bf, c);
            }
            
            continue;
        }
        else if (flags & F_READ_STRING)
        {
            if (!(flags & F_READ_ESCAPE) && c == '"')
                nflags &= ~F_READ_STRING;
        }
        else
        {
            if (c == '(')
            {
                ++rr->paren;
                nflags &= ~F_READ_START;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == ')')
            {
                --rr->paren;
                nflags &= ~F_READ_START;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == '"')
            {
                nflags |= F_READ_STRING;
                nflags &= ~F_READ_SPACE;
            }
            else if (c == ';')
            {
                flags |= F_READ_COMMENT;
                flags &= ~F_READ_SPACE;
                continue;
            }
            else if (c == '\n')
            {
                if (!(flags & F_READ_START))
                    nflags |= F_READ_SPACE;
            }
            else if (isspace(c))
            {
                if (flags & F_READ_START)
                    continue;
   
                if (!(flags & F_READ_SPACE) && rr->paren > 0)
                    c = ' ';
                
                nflags |= F_READ_SPACE;
            }
            else
            {
                nflags &= ~F_READ_SPACE;
            }
        }

        if (c == '\\')   nflags |= F_READ_ESCAPE;
        else             nflags &= ~F_READ_ESCAPE;

        writec_buffer(bf, c);
        if ((!(flags & F_READ_START) || (nflags & F_READ_SPACE)) && rr->paren == 0)
        {
            loc->row = nrow;
            loc->col = ncol;
            rr->status = READ_RESULT_SUCCESS;
            rr->flags = flags;
            break;
        }

        flags = nflags;
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
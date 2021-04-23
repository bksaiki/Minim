#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/buffer.h"
#include "parser.h"

#define open_paren(x)       (x == '(' || x == '[' || x == '{')
#define closed_paren(x)     (x == ')' || x == ']' || x == '{')
#define normal_char(x)      (x && !open_paren(x) && !closed_paren(x) && !isspace(x))

#define IF_STR_EQUAL_REPLACE(x, str, r)                     \
{                                                           \
    if (strcmp(x, str) == 0)                                \
    {                                                       \
        x = realloc(x, (strlen(r) + 1) * sizeof(char));     \
        strcpy(x, r);                                       \
        return true;                                        \
    }                                                       \
}

//
//  Helper functions for reading
//

// Forward declaration
static SyntaxNode *read_top(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror);

static void update_table(char c, ReadTable *ptable)
{
    ++ptable->idx;
    if (c == '\n')
    {
        ++ptable->row;
        ptable->col = 0;
    }
    else
    {
        ++ptable->col;
    }
}

static char advance_to_token(FILE *file, ReadTable *ptable)
{
    char c;

    c = fgetc(file);
    while (isspace(c) && c != ptable->eof)
    {
        update_table(c, ptable);
        c = fgetc(file);
    }

    if (c == ';')
    {
        update_table(c, ptable);
        c = fgetc(file);
        while (c != '\n' && c != ptable->eof)
        {
            update_table(c, ptable);
            c = fgetc(file);
        }

        if (c == '\n' && c != ptable->eof)
        {
            update_table(c, ptable);
            c = advance_to_token(file, ptable);
        }
    }

    return c;
}

static bool expand_shorthand_syntax(SyntaxNode *node)
{
    IF_STR_EQUAL_REPLACE(node->sym, "f", "false");
    IF_STR_EQUAL_REPLACE(node->sym, "t", "true");

    return false;
}

static void expand_list(SyntaxNode *node)
{
    if (node->childc == 3 && node->children[1]->sym &&
        strcmp(node->children[1]->sym, ".") == 0)
    {
        free_syntax_node(node->children[1]);
        node->children[1] = node->children[2];

        node->children = realloc(node->children, 2 * sizeof(SyntaxNode*));
        node->childc = 2;
        node->type = SYNTAX_NODE_PAIR;
    }
    else if (node->childc == 5 && node->children[1]->sym && node->children[3]->sym &&
             strcmp(node->children[1]->sym, ".") == 0 &&
             strcmp(node->children[3]->sym, ".") == 0)
    {
        free_syntax_node(node->children[1]);
        free_syntax_node(node->children[3]);
        node->children[1] = node->children[0];
        node->children[0] = node->children[2];
        node->children[2] = node->children[4];
        
        node->children = realloc(node->children, 3 * sizeof(SyntaxNode*));
        node->childc = 3;
        node->type = SYNTAX_NODE_LIST;
    }
}

static SyntaxNode *read_datum(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node;
    SyntaxLoc *loc;
    Buffer *bf;
    char c;
    
    init_buffer(&bf);
    c = fgetc(file);
    while (c != ptable->eof && normal_char(c))
    {
        writec_buffer(bf, c);
        update_table(c, ptable);
        c = fgetc(file);
    }

    if (c == ptable->eof)   ptable->flags |= SYNTAX_NODE_FLAG_EOF;
    else                    ungetc(c, file);           

    init_syntax_node(&node, SYNTAX_NODE_DATUM);
    trim_buffer(bf);
    node->sym = release_buffer(bf);
    free_buffer(bf);

    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node, loc);

    return node;
}

static SyntaxNode *read_string(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node;
    SyntaxLoc *loc;
    Buffer *bf;
    char c;

    init_buffer(&bf);
    writec_buffer(bf, '"');
    c = fgetc(file);
    update_table(c, ptable);

    while (c != '"')
    {
        if (c == ptable->eof)
        {
            Buffer *bf2;

            init_buffer(&bf2);
            writes_buffer(bf2, "unexpected end of input");
            trim_buffer(bf2);

            ptable->flags |= SYNTAX_NODE_FLAG_BAD;
            init_syntax_node(perror, SYNTAX_NODE_DATUM);
            (*perror)->sym = release_buffer(bf2);
            free_buffer(bf2);
            break;
        }

        writec_buffer(bf, c);
        if (c == '\\')
        {
            c = fgetc(file);
            update_table(c, ptable);
            if (c == ptable->eof)
                continue;
            writec_buffer(bf, c);
        }

        c = fgetc(file);
        update_table(c, ptable);
    }

    if (c == '"')
        writec_buffer(bf, c);

    init_syntax_node(&node, SYNTAX_NODE_DATUM);
    trim_buffer(bf);
    node->sym = release_buffer(bf);
    free_buffer(bf);

    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node, loc);

    return node;
}

static SyntaxNode *read_quote(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node;
    SyntaxLoc *loc;

    init_syntax_node(&node, SYNTAX_NODE_LIST);
    node->children = malloc(2 * sizeof(SyntaxNode*));
    node->childc = 2;

    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node, loc);
    
    init_syntax_node(&node->children[0], SYNTAX_NODE_DATUM);
    node->children[0]->sym = malloc(6 * sizeof(char));
    strcpy(node->children[0]->sym, "quote");

    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node->children[0], loc);

    node->children[1] = read_top(file, name, ptable, perror);
    return node;
}

static SyntaxNode *read_list(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node, *tmp;
    SyntaxLoc *loc;
    char c;

    init_syntax_node(&node, SYNTAX_NODE_LIST);
    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node, loc);

    while (1)
    {
        tmp = read_top(file, name, ptable, perror);
        if (tmp)
        {
            ++node->childc;
            node->children = realloc(node->children, node->childc * sizeof(SyntaxNode*));
            node->children[node->childc - 1] = tmp;
        }

        if (ptable->flags & SYNTAX_NODE_FLAG_EOF)
        {
            if (ptable->flags & SYNTAX_NODE_FLAG_WAIT)
            {
                printf("  ");
                ptable->flags ^= SYNTAX_NODE_FLAG_EOF;
            }
            else
            {
                Buffer *bf;

                init_buffer(&bf);
                writes_buffer(bf, "unexpected end of input");
                trim_buffer(bf);

                ptable->flags |= SYNTAX_NODE_FLAG_BAD;
                init_syntax_node(perror, SYNTAX_NODE_DATUM);
                (*perror)->sym = release_buffer(bf);
                free_buffer(bf);
                break;
            }
        }

        c = advance_to_token(file, ptable);
        if (closed_paren(c))
        {
            update_table(c, ptable);
            break;
        }
        else
        {
            ungetc(c, file);
        }
    }

    expand_list(node);
    return node;
}

static SyntaxNode *read_vector(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node, *tmp;
    SyntaxLoc *loc;
    char c;

    init_syntax_node(&node, SYNTAX_NODE_VECTOR);
    init_syntax_loc(&loc, name);
    loc->col = ptable->col;
    loc->row = ptable->row;
    ast_add_syntax_loc(node, loc);

    while (1)
    {
        tmp = read_top(file, name, ptable, perror);
        if (tmp)
        {
            ++node->childc;
            node->children = realloc(node->children, node->childc * sizeof(SyntaxNode*));
            node->children[node->childc - 1] = tmp;
        }

        c = advance_to_token(file, ptable);
        if (closed_paren(c))
        {
            update_table(c, ptable);
            break;
        }
        else if (c == ptable->eof)
        {
            if (ptable->flags & SYNTAX_NODE_FLAG_WAIT)
            {
                printf("  ");
            }
            else
            {
                Buffer *bf;

                init_buffer(&bf);
                writes_buffer(bf, "unexpected end of input");
                trim_buffer(bf);

                ptable->flags |= SYNTAX_NODE_FLAG_BAD;
                init_syntax_node(perror, SYNTAX_NODE_DATUM);
                (*perror)->sym = release_buffer(bf);
                free_buffer(bf);
                break;
            }
        }
        else
        {
            ungetc(c, file);
        }
    }

    return node;
}

static SyntaxNode *read_top(FILE *file, const char *name, ReadTable *ptable, SyntaxNode **perror)
{
    SyntaxNode *node;
    char c, n;

    c = advance_to_token(file, ptable);
    if (c == '\'')
    {
        update_table(c, ptable);
        node = read_quote(file, name, ptable, perror);
    }
    else if (c == '#')
    {
        update_table(c, ptable);
        n = fgetc(file);
        if (n == '(')
        {
            update_table(c, ptable);
            node = read_vector(file, name, ptable, perror);

            c = advance_to_token(file, ptable);
            if (c == ptable->eof)   ptable->flags |= SYNTAX_NODE_FLAG_EOF;
            else                    ungetc(c, file);
        }
        else
        {
            ungetc(n, file);
            node = read_datum(file, name, ptable, perror);
            if (!expand_shorthand_syntax(node))
            {
                Buffer *bf;

                init_buffer(&bf);
                writef_buffer(bf, "bad syntax: #~s", node->sym);
                trim_buffer(bf);

                ptable->flags |= SYNTAX_NODE_FLAG_BAD;
                init_syntax_node(perror, SYNTAX_NODE_DATUM);
                (*perror)->sym = release_buffer(bf);
                free_buffer(bf);
            }
        }
    }
    else if (open_paren(c))
    {
        update_table(c, ptable);
        node = read_list(file, name, ptable, perror);

        c = advance_to_token(file, ptable);
        if (c == ptable->eof)   ptable->flags |= SYNTAX_NODE_FLAG_EOF;
        else                    ungetc(c, file);
    }
    else if (closed_paren(c) && c == ptable->eof) // nonsense 
    {
        printf("Should not be here!");
        return NULL;
    }
    else if (c == '"')      // string
    {
        node = read_string(file, name, ptable, perror);
        c = advance_to_token(file, ptable);
        if (c == ptable->eof)   ptable->flags |= SYNTAX_NODE_FLAG_EOF;
        else                    ungetc(c, file);
    }
    else if (c == ptable->eof) // empty input
    {
        if (ptable->flags & SYNTAX_NODE_FLAG_WAIT)
        {
            printf("  ");
            ptable->flags ^= SYNTAX_NODE_FLAG_EOF;
            node = read_top(file, name, ptable, perror);
        }
        else
        {
            Buffer *bf;

            init_buffer(&bf);
            writes_buffer(bf, "unexpected end of input");
            trim_buffer(bf);

            ptable->flags |= SYNTAX_NODE_FLAG_BAD;
            init_syntax_node(perror, SYNTAX_NODE_DATUM);
            (*perror)->sym = release_buffer(bf);
            free_buffer(bf);
            return NULL;
        }
    }
    else if (normal_char(c))
    {
        ungetc(c, file);
        node = read_datum(file, name, ptable, perror);
    }
    else
    {
        ungetc(c, file);
        return NULL;
    }

    return node;
}

//
// Exported
//

int minim_parse_port(FILE *file, const char *name, SyntaxNode **psyntax, char eof, bool wait)
{
    SyntaxNode *node, *err;
    ReadTable table;

    table.idx = 0;
    table.row = 0;
    table.col = 0;
    table.eof = eof;
    table.flags = (wait ? SYNTAX_NODE_FLAG_WAIT : 0x0);

    node = read_top(file, name, &table, &err);
    if (table.flags & SYNTAX_NODE_FLAG_BAD)
    {
        if (node)       free_syntax_node(node);
        else            node = err;
    }
    else if (~table.flags & SYNTAX_NODE_FLAG_EOF)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "unexpected: '~c'", fgetc(file));
        trim_buffer(bf);

        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = release_buffer(bf);
        free_buffer(bf);
    }

    *psyntax = node;
    return 0;
}

int minim_parse_port2(FILE *file, const char *name, SyntaxNode **psyntax, ReadTable *table)
{
    SyntaxNode *node, *err;

    node = read_top(file, name, table, &err);
    if (table->flags & SYNTAX_NODE_FLAG_BAD)
    {
        if (node)       free_syntax_node(node);
        else            node = err;
    }

    *psyntax = node;
    return 0;
}

//
//  Visible functions
// 

int parse_str(const char* str, SyntaxNode** psyntax)
{
    FILE *tmp;
    int status;

    tmp = tmpfile();
    fputs(str, tmp);
    rewind(tmp);
    status = minim_parse_port(tmp, "test", psyntax, EOF, false);
    fclose(tmp);

    return status;
}
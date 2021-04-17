#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/buffer.h"
#include "reader.h"

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

static void init_read_node(ReadNode **pnode, ReadNodeType type)
{
    ReadNode *node = malloc(sizeof(ReadNode));

    node->children = NULL;
    node->childc = 0;
    node->sym = NULL;
    node->type = type;
    *pnode = node;
}

static void update_table(char c, SyntaxTable *ptable)
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

static void check_char(char c, SyntaxTable *ptable)
{
    if (c == ptable->eof)
        ptable->flags |= READ_NODE_FLAG_EOF;
}

static char advance_to_token(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    char c;

    c = fgetc(file);
    while (isspace(c))
    {
        update_table(c, ptable);
        c = fgetc(file);
    }

    if (c == ';')
    {
        update_table(c, ptable);
        c = fgetc(file);
        while (c != '\n')
        {
            update_table(c, ptable);
            c = fgetc(file);
        }
    }

    return c;
}

static bool expand_shorthand_syntax(ReadNode *node)
{
    IF_STR_EQUAL_REPLACE(node->sym, "f", "false");
    IF_STR_EQUAL_REPLACE(node->sym, "t", "true");

    return false;
}

static void expand_list(ReadNode *node)
{
    if (node->childc == 3 && node->children[1]->sym &&
        strcmp(node->children[1]->sym, ".") == 0)
    {
        node->children[1] = node->children[2];
        node->children = realloc(node->children, 2 * sizeof(ReadNode*));
        node->childc = 2;
        node->type = READ_NODE_PAIR;
    }
    else if (node->childc == 5 && node->children[1]->sym && node->children[3]->sym &&
             strcmp(node->children[1]->sym, ".") == 0 &&
             strcmp(node->children[3]->sym, ".") == 0)
    {
        node->children[1] = node->children[2];
        node->children[2] = node->children[4];
        node->children = realloc(node->children, 3 * sizeof(ReadNode*));
        node->childc = 3;
        node->type = READ_NODE_LIST;
    }
}

// Forward declaration
static ReadNode *read_top(FILE *file, SyntaxTable *ptable, ReadNode **perror);

static ReadNode *read_datum(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    ReadNode *node;
    Buffer *bf;
    char c;
    
    init_buffer(&bf);
    c = fgetc(file);
    while (!ptable->flags && normal_char(c))
    {
        writec_buffer(bf, c);
        update_table(c, ptable);
        c = fgetc(file);
    }

    check_char(c, ptable);
    ungetc(c, file);

    init_read_node(&node, READ_NODE_DATUM);
    trim_buffer(bf);
    node->sym = release_buffer(bf);
    free_buffer(bf);

    return node;
}

static ReadNode *read_quote(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    ReadNode *node;

    init_read_node(&node, READ_NODE_LIST);
    node->children = malloc(2 * sizeof(ReadNode*));
    node->childc = 2;
    
    init_read_node(&node->children[0], READ_NODE_DATUM);
    node->children[0]->sym = malloc(6 * sizeof(char));
    strcpy(node->children[0]->sym, "quote");

    node->children[1] = read_top(file, ptable, perror);
    return node;
}

static ReadNode *read_list(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    ReadNode *node, *tmp;
    char c;

    init_read_node(&node, READ_NODE_LIST);
    while (1)
    {
        tmp = read_top(file, ptable, perror);

        if (tmp)
        {
            ++node->childc;
            node->children = realloc(node->children, node->childc * sizeof(ReadNode*));
            node->children[node->childc - 1] = tmp;
        }

        c = fgetc(file);
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

static ReadNode *read_vector(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    ReadNode *node, *tmp;
    char c;

    init_read_node(&node, READ_NODE_VECTOR);
    while (1)
    {
        tmp = read_top(file, ptable, perror);

        if (tmp)
        {
            ++node->childc;
            node->children = realloc(node->children, node->childc * sizeof(ReadNode*));
            node->children[node->childc - 1] = tmp;
        }

        c = fgetc(file);
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

    return node;
}

static ReadNode *read_top(FILE *file, SyntaxTable *ptable, ReadNode **perror)
{
    ReadNode *node;
    char c, n;

    c = advance_to_token(file, ptable, perror);
    if (c == '\'')
    {
        update_table(c, ptable);
        node = read_quote(file, ptable, perror);
    }
    else if (c == '#')
    {
        update_table(c, ptable);
        n = fgetc(file);
        if (n == '(')
        {
            update_table(c, ptable);
            node = read_vector(file, ptable, perror);
        }
        else
        {
            ungetc(n, file);
            node = read_datum(file, ptable, perror);
            if (!expand_shorthand_syntax(node))
                ptable->flags |= READ_NODE_FLAG_BAD;
        }
    }
    else if (open_paren(c))
    {
        update_table(c, ptable);
        node = read_list(file, ptable, perror);
    }
    else if (closed_paren(c)) // possibly closing a list
    {
        ungetc(c, file);
        return NULL;
    }
    else if (c == ptable->eof) // empty input
    {
        return NULL;
    }
    else
    {
        ungetc(c, file);
        node = read_datum(file, ptable, perror);
    }

    return node;
}

//
// Exported
//

void print_syntax(ReadNode *node)
{
    if (node->childc > 0)
    {
        printf("(");
        for (size_t i = 0; i < node->childc; ++i)
            print_syntax(node->children[i]);
        printf(")");
    }
    else
    {
        printf("%s, ", node->sym);
    }
}

ReadNode *minim_read_str(FILE *file, ReadNode **perror)
{
    ReadNode *node;
    SyntaxTable table;

    table.idx = 0;
    table.row = 0;
    table.col = 0;
    table.eof = '\n';
    table.flags = 0x0;

    do node = read_top(file, &table, perror);
    while (!node);
    
    if (~table.flags & READ_NODE_FLAG_EOF)
    {
        char str[256];

        fgets(str, 255, file);
        printf("Unexpected characters after expression: %s", str);
    }
    else if (table.flags & READ_NODE_FLAG_BAD)
    {
        printf("Syntax failure\n");
    }

    printf("Read %zu chars\n", table.idx);
    printf("Finished at (%zu, %zu)\n", table.row, table.col);
    return node;
}
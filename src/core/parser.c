#include <ctype.h>
#include <string.h>

#include "../gc/gc.h"
#include "../common/path.h"
#include "error.h"
#include "parser.h"
#include "port.h"

#define open_paren(x)       (x == '(' || x == '[' || x == '{')
#define closed_paren(x)     (x == ')' || x == ']' || x == '{')
#define normal_char(x)      (x && !open_paren(x) && !closed_paren(x) && !isspace(x))

#define IF_STR_EQUAL_REPLACE(x, str, r)                                 \
{                                                                       \
    if (strcmp(x, str) == 0)                                            \
    {                                                                   \
        x = GC_realloc_atomic(x, (strlen(r) + 1) * sizeof(char));       \
        strcpy(x, r);                                                   \
        return true;                                                    \
    }                                                                   \
}

#define ADD_SYNTAX_LOC(node, p)                                 \
{                                                               \
    init_syntax_loc(&(node)->loc, MINIM_PORT_NAME(port));       \
    (node)->loc->row = MINIM_PORT_ROW(port);                    \
    (node)->loc->col = MINIM_PORT_COL(port);                    \
}

//
//  Helper functions for reading
//

// Forward declaration
static SyntaxNode *read_top(MinimObject *port, SyntaxNode **perr, uint8_t flags);

static char next_char(MinimObject *port)
{
    char c = next_ch(port);

    while (isspace(c) && c != port_eof(port))
    {
        update_port(port, c);
        c = next_ch(port);
    }

    if (c == ';')
    {
        update_port(port, c);
        c = next_ch(port);
        while (c != '\n' && c != port_eof(port))
        {
            update_port(port, c);
            c = next_ch(port);
        }

        if (c == '\n' && c != port_eof(port))
        {
            update_port(port, c);
            c = next_char(port);
        }
    }

    return c;
}

static SyntaxNode *list_error()
{
    SyntaxNode *err;
    const char *msg = "illegal use of `.`";

    init_syntax_node(&err, SYNTAX_NODE_DATUM);
    err->sym = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
    strcpy(err->sym, msg);
    return err;
}

static SyntaxNode *end_of_input_error()
{
    SyntaxNode *err;
    const char *msg = "unexpected end of input";

    init_syntax_node(&err, SYNTAX_NODE_DATUM);
    err->sym = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
    strcpy(err->sym, msg);
    return err;
}

static bool expand_syntax(SyntaxNode *node)
{
    IF_STR_EQUAL_REPLACE(node->sym, "f", "false");
    IF_STR_EQUAL_REPLACE(node->sym, "t", "true");
    return false;
}

static bool expand_list(SyntaxNode *node, SyntaxNode **perr)
{
    bool ellipse;

    if (node->childc == 3 && node->children[1]->sym &&      // cons cell
        strcmp(node->children[1]->sym, ".") == 0)
    {
        node->children[1] = node->children[2];
        node->children = GC_realloc(node->children, 2 * sizeof(SyntaxNode*));
        node->childc = 2;
        node->type = SYNTAX_NODE_PAIR;

        return true;
    }
    
    ellipse = false;
    for (size_t i = 0; i < node->childc; ++i)       // (<elem> ... . <front> . <elem> ...)
    {
        if (node->children[i]->sym && strcmp(node->children[i]->sym, ".") == 0)
        {
            if (ellipse)
            {
                *perr = list_error();
                return false;
            }
            else if (i + 2 < node->childc && node->children[i + 2]->sym &&
            strcmp(node->children[i + 2]->sym, ".") == 0)
            {
                if (node->children[i + 1]->sym && strcmp(node->children[i + 1]->sym, ".") == 0)
                {
                    *perr = list_error();
                    return false;
                }

                for (size_t j = i; j > 0; --j)
                    node->children[j] = node->children[j - 1];

                node->children[0] = node->children[i + 1];
                for (size_t j = i + 3; j < node->childc; ++j)
                    node->children[j - 2] = node->children[j];

                node->childc -= 2;
                node->children = GC_realloc(node->children, node->childc * sizeof(SyntaxNode*));
                ellipse = true;
            }
            else if (i + 2 != node->childc)
            {
                *perr = list_error();
                return false;
            }
        }
    }

    return true;
}

static SyntaxNode *read_1ary(MinimObject *port, SyntaxNode **perr, const char *op, uint8_t flags)
{
    SyntaxNode *node;

    init_syntax_node(&node, SYNTAX_NODE_LIST);
    node->children = GC_alloc(2 * sizeof(SyntaxNode*));
    node->childc = 2;
    ADD_SYNTAX_LOC(node, port);

    init_syntax_node(&node->children[0], SYNTAX_NODE_DATUM);
    node->children[0]->sym = GC_alloc_atomic((strlen(op) + 1) * sizeof(char));
    strcpy(node->children[0]->sym, op);
    ADD_SYNTAX_LOC(node, port);

    node->children[1] = read_top(port, perr, flags);
    return node;
}

static SyntaxNode *read_nested(MinimObject *port, SyntaxNode **perr, SyntaxNodeType type, uint8_t flags)
{
    SyntaxNode *node, *next;
    char c;

    init_syntax_node(&node, type);
    ADD_SYNTAX_LOC(node, port);

    c = next_char(port);
    if (c == ')')
    {
        update_port(port, c);
    }
    else
    {
        put_back(port, c);
        while (1)
        {
            next = read_top(port, perr, flags);
            if (*perr)   return NULL;

            if (next)
            {
                ++node->childc;
                node->children = GC_realloc(node->children, node->childc * sizeof(SyntaxNode*));
                node->children[node->childc - 1] = next;
            }

            c = next_char(port);
            if (closed_paren(c))
            {
                update_port(port, c);
                break;
            }
            else if (c == port_eof(port))
            {
                if (flags & READ_FLAG_WAIT)
                {
                    printf(" ");
                }
                else
                {
                    *perr = end_of_input_error();
                    break;
                }
            }
            else
            {
                put_back(port, c);
            }
        }

        if (type == SYNTAX_NODE_LIST)
        {
            if (!expand_list(node, perr))
                return NULL;
        }
    }

    c = next_char(port);
    if (c == port_eof(port))    MINIM_PORT_MODE(port) &= ~MINIM_PORT_MODE_READY;
    else                        put_back(port, c);

    return node;
}

static SyntaxNode *read_datum(MinimObject *port, SyntaxNode **perr, uint8_t flags)
{
    SyntaxNode *node;
    Buffer *bf;
    char c;

    init_buffer(&bf);
    c = next_ch(port);
    while (c != port_eof(port) && normal_char(c))
    {
        writec_buffer(bf, c);
        update_port(port, c);
        c = next_ch(port);
    }

    if (c == port_eof(port))    update_port(port, c);
    else                        put_back(port, c);

    trim_buffer(bf);
    init_syntax_node(&node, SYNTAX_NODE_DATUM);
    node->sym = get_buffer(bf);
    ADD_SYNTAX_LOC(node, port);

    return node;
}

static SyntaxNode *read_string(MinimObject *port, SyntaxNode **perr, uint8_t flags)
{
    SyntaxNode *node;
    Buffer *bf;
    char c;

    init_buffer(&bf);
    writec_buffer(bf, '"');
    c = next_ch(port);
    update_port(port, c);
    while (c != '"')
    {
        if (c == port_eof(port))
        {
            *perr = end_of_input_error();
            return NULL;
        }

        writec_buffer(bf, c);
        if (c == '\\')
        {
            c = next_ch(port);
            update_port(port, c);
            if (c == port_eof(port))
                continue;

            writec_buffer(bf, c);
        }

        c = next_ch(port);
        update_port(port, c);
    }

    if (c == '"')
        writec_buffer(bf, c);

    trim_buffer(bf);
    init_syntax_node(&node, SYNTAX_NODE_DATUM);
    node->sym = get_buffer(bf);
    ADD_SYNTAX_LOC(node, port);

    return node;
}

static SyntaxNode *read_char(MinimObject *port, SyntaxNode **perr, uint8_t flags)
{
    SyntaxNode *node;
    Buffer *bf;
    char c;

    init_buffer(&bf);
    writes_buffer(bf, "#\\");
    c = next_ch(port);
    update_port(port, c);

    writec_buffer(bf, c);
    // if (c == 'u')            unicode scalar
    // {
    //     c = fgetc(file);
    // }

    trim_buffer(bf);
    init_syntax_node(&node, SYNTAX_NODE_DATUM);
    node->sym = get_buffer(bf);
    ADD_SYNTAX_LOC(node, port);

    c = next_ch(port);
    if (c == port_eof(port))    update_port(port, c);
    else                        put_back(port, c);

    return node;
}

static SyntaxNode *read_top(MinimObject *port, SyntaxNode **perr, uint8_t flags)
{
    SyntaxNode *node;
    Buffer *bf;
    char c, n;

    c = next_char(port);
    if (c == '\'')
    {
        update_port(port, c);
        node = read_1ary(port, perr, "quote", flags);
    }
    else if (c == '`')
    {
        update_port(port, c);
        node = read_1ary(port, perr, "quasiquote", flags);
    }
    else if (c == ',')
    {
        update_port(port, c);
        node = read_1ary(port, perr, "unquote", flags);
    }
    else if (c == '#')
    {
        update_port(port, c);
        n = next_ch(port);
        if (n == '(')
        {
            update_port(port, n);
            node = read_nested(port, perr, SYNTAX_NODE_VECTOR, flags);

            c = next_char(port);
            if (c == port_eof(port))    MINIM_PORT_MODE(port) &= ~MINIM_PORT_MODE_READY;
            else                        put_back(port, c);
        }
        else if (n == '\'')
        {
            update_port(port, n);
            node = read_1ary(port, perr, "syntax", flags);
        }
        else if (n == '\\')
        {
            update_port(port, n);
            node = read_char(port, perr, flags);   
        }
        else
        {
            put_back(port, n);
            node = read_datum(port, perr, flags);
            if (!expand_syntax(node))
            {
                Buffer *bf;

                init_buffer(&bf);
                writef_buffer(bf, "bad syntax: #~s", node->sym);
                trim_buffer(bf);

                init_syntax_node(perr, SYNTAX_NODE_DATUM);
                (*perr)->sym = get_buffer(bf);
            }
        }
    }
    else if (open_paren(c))
    {
        update_port(port, c);
        node = read_nested(port, perr, SYNTAX_NODE_LIST, flags);
    }
    else if (c == '"')
    {
        node = read_string(port, perr, flags);
        c = next_char(port);
        if (c == port_eof(port))    MINIM_PORT_MODE(port) &= ~MINIM_PORT_MODE_READY;
        else                        put_back(port, c);
    }
    else if (c == port_eof(port))
    {
        if (flags & READ_FLAG_WAIT)
        {
            printf(" ");
            node = read_top(port, perr, flags);
        }
        else
        {
            *perr = end_of_input_error();
            return NULL;
        }
    }
    else if (normal_char(c))
    {
        put_back(port, c);
        node = read_datum(port, perr, flags);
    }
    else
    {
        init_buffer(&bf);
        writes_buffer(bf, "parsing failed");
        trim_buffer(bf);
        init_syntax_node(perr, SYNTAX_NODE_DATUM);
        (*perr)->sym = get_buffer(bf);
        return NULL;
    }

    return node;
}

//
// Exported
//

SyntaxNode *minim_parse_port(MinimObject *port, SyntaxNode **perr, uint8_t flags)
{
    SyntaxNode *node;

    *perr = NULL;
    node = read_top(port, perr, flags);
    if (*perr)
    {
        return NULL;
    }
    else if (!node)
    {
        Buffer *bf;
        char c;
        
        c = next_ch(port);
        update_port(port, c);

        init_buffer(&bf);
        writef_buffer(bf, "unexpected: '~c'", c);
        trim_buffer(bf);

        init_syntax_node(perr, SYNTAX_NODE_DATUM);
        (*perr)->sym = get_buffer(bf);

        c = next_ch(port);
        if (c == port_eof(port))    update_port(port, c);
        else                        put_back(port, c);

        return NULL;
    }

    return node;
}

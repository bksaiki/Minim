#include <ctype.h>

#include "minimpriv.h"

#define open_paren(x)       (x == '(' || x == '[' || x == '{')
#define closed_paren(x)     (x == ')' || x == ']' || x == '{')
#define normal_char(x)      (x && !open_paren(x) && !closed_paren(x) && !isspace(x))

#define IF_STR_EQUAL_REPLACE(stx, str, r)           \
{                                                   \
    if (strcmp(MINIM_STX_SYMBOL(stx), str) == 0)    \
    {                                               \
        MINIM_STX_VAL(stx) = intern(r);             \
        return true;                                \
    }                                               \
}

#define START_SYNTAX_LOC(loc, p)                                \
{                                                               \
    loc = init_syntax_loc(minim_symbol(MINIM_PORT_NAME(p)),     \
                          MINIM_PORT_ROW(p),                    \
                          MINIM_PORT_COL(p),                    \
                          MINIM_PORT_POS(p),                    \
                          0);                                   \
}

#define END_SYNTAX_LOC(loc, p)                                  \
{                                                               \
    (loc)->span = MINIM_PORT_POS(p) - (loc)->pos;               \
}

#define CHECK_EOF(p, c)                                                         \
{                                                                               \
    c = next_char(p);                                                           \
    if ((c) == port_eof(p))     MINIM_PORT_MODE(p) &= ~MINIM_PORT_MODE_READY;   \
    else                        put_back(p, c);                                 \
}

//
//  Helper functions for reading
//

// Forward declaration
static MinimObject *read_top(MinimObject *port, MinimObject **perr, uint8_t flags);

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

#define ILLEGAL_DOT_MSG_LEN         19
#define UNEXPECTED_EOF_MSG_LEN      24

static MinimObject *list_error()
{
    return minim_symbol("illegal use of `.`");
}

static MinimObject *end_of_input_error()
{
    return minim_symbol("unexpected end of input");
}

static MinimObject *bad_syntax_error(const char *syntax)
{
    Buffer *bf;

    init_buffer(&bf);
    writef_buffer(bf, "bad syntax #~s", syntax);
    return minim_symbol(get_buffer(bf));
}

static MinimObject *unexpected_char_error(char ch)
{
    Buffer *bf;

    init_buffer(&bf);
    writef_buffer(bf, "unexpected char ~c", ch);
    return minim_symbol(get_buffer(bf));
}

static bool verify_list(MinimObject *stx, MinimObject **perr)
{
    MinimObject *h, *t, *i;
    size_t list_len = syntax_list_len(stx);
    bool dot = false;

    if (list_len == 0)
        return true;
    
    h = MINIM_STX_VAL(stx);
    MINIM_TAIL(t, h);
    if (MINIM_STX_SYMBOLP(h) && strcmp(MINIM_STX_SYMBOL(h), ".") == 0)  // dot cannot be first element
    {
        *perr = list_error();
        return false;
    }

    t = MINIM_CAR(t);
    if (MINIM_STX_SYMBOLP(t) && strcmp(MINIM_STX_SYMBOL(t), ".") == 0)  // dot cannot be last element
    {
        *perr = list_error();
        return false;
    }

    i = MINIM_STX_CDR(stx);
    while (MINIM_OBJ_PAIRP(i))
    {
        h = MINIM_CAR(i);
        if (MINIM_STX_SYMBOLP(h) && strcmp(MINIM_STX_SYMBOL(h), ".") == 0)  // dot found
        {
            if (dot)
            {
                *perr = list_error();
                return false;
            }
            else
            {
                dot = true;
                t = MINIM_CDR(i);
                if (MINIM_OBJ_PAIRP(t))
                {
                    h = MINIM_CAR(t);
                    if (!MINIM_STX_SYMBOLP(h) || strcmp(MINIM_STX_SYMBOL(h), ".") != 0)     // no dot
                    { 
                        t = MINIM_CDR(t);
                        if (MINIM_OBJ_PAIRP(t))
                        {
                            h = MINIM_CAR(t);
                            if (MINIM_STX_SYMBOLP(h) && strcmp(MINIM_STX_SYMBOL(h), ".") == 0)  // infix dot found
                            {
                                i = MINIM_CDR(t);
                                continue;
                            }
                        }
                    }
                }
                
            }
        }

        i = MINIM_CDR(i);
    }

    return true;
}

static bool expand_syntax(MinimObject *stx)
{
    IF_STR_EQUAL_REPLACE(stx, "f", "false");
    IF_STR_EQUAL_REPLACE(stx, "t", "true");
    return false;
}

static bool expand_list(MinimObject *stx, MinimObject **perr)
{
    size_t list_len;

    if (!verify_list(stx, perr))
        return false;
    
    list_len = syntax_list_len(stx);
    if (list_len == 3)          // possible cons cell
    {
        MinimObject *h, *t, *d;

        h = MINIM_STX_VAL(stx);
        t = MINIM_CDR(h);
        d = MINIM_CAR(t);
        
        // cons cell
        if (MINIM_STX_SYMBOLP(d) && strcmp(MINIM_STX_SYMBOL(d), ".") == 0)
            MINIM_CDR(h) = MINIM_CADR(t);    // remove dot
    }
    else if (list_len == 4)     // only can terminate in cons cell
    {
        MinimObject *l, *t, *d;

        l = MINIM_CDR(MINIM_STX_VAL(stx));
        t = MINIM_CDR(l);
        d = MINIM_CAR(t);

        // cons cell
        if (MINIM_STX_SYMBOLP(d) && strcmp(MINIM_STX_SYMBOL(d), ".") == 0)
            MINIM_CDR(l) = MINIM_CADR(t);    // remove dot

    }
    else if (list_len >= 5)     // possible infix
    {
        MinimObject *h, *t, *i1, *i2, *i3;

        h = MINIM_STX_VAL(stx);     // list
        t = MINIM_CDR(h);
        i1 = MINIM_CAR(t);          // cadr of list
        t = MINIM_CDR(t);
        i2 = MINIM_CAR(t);          // caddr of list
        t = MINIM_CDR(t);
        
        while (!minim_nullp(t))
        {
            i3 = MINIM_CAR(t);      // cadddr of list
            if ((MINIM_STX_SYMBOLP(i1) && strcmp(MINIM_STX_SYMBOL(i1), ".") == 0) &&
                (MINIM_STX_SYMBOLP(i3) && strcmp(MINIM_STX_SYMBOL(i3), ".") == 0))
            {
                MINIM_STX_VAL(stx) = minim_cons(i2, MINIM_STX_VAL(stx));
                MINIM_CDR(h) = MINIM_CDR(t);

                return true;
            }

            h = MINIM_CDR(h);
            i1 = i2;
            i2 = i3;
            t = MINIM_CDR(t);
        }

        if (MINIM_STX_SYMBOLP(i1) && strcmp(MINIM_STX_SYMBOL(i1), ".") == 0)
            MINIM_CDR(h) = i2;
    }

    return true;
}

static MinimObject *read_1ary(MinimObject *port, MinimObject **perr, const char *name, uint8_t flags)
{
    MinimObject *arg, *sym;
    SyntaxLoc *loc;
    
    sym = intern(name);
    START_SYNTAX_LOC(loc, port);
    sym = minim_ast(sym, loc);
    END_SYNTAX_LOC(loc, port);

    START_SYNTAX_LOC(loc, port);
    arg = read_top(port, perr, flags);
    arg = minim_ast(minim_cons(sym, minim_cons(arg, minim_null)), loc);
    END_SYNTAX_LOC(MINIM_STX_LOC(sym), port);
    return arg;
}

static MinimObject *read_list(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *lst, *it, *el;
    SyntaxLoc *loc;
    char c;

    START_SYNTAX_LOC(loc, port);
    c = next_char(port);
    if (c == ')')           // empty list
    {
        update_port(port, c);
        END_SYNTAX_LOC(loc, port);
        return minim_ast(minim_null, loc);
    }

    put_back(port, c);
    lst = NULL;
    while (true)
    {
        el = read_top(port, perr, flags);
        if (*perr)      // parse error
        {
            END_SYNTAX_LOC(loc, port);
            return minim_ast(minim_null, loc);
        }

        if (lst != NULL)    // list already constructed
        {
            MINIM_CDR(it) = minim_cons(el, minim_null);
            it = MINIM_CDR(it);
        }
        else                // list not constructed
        {
            lst = minim_cons(el, minim_null);
            it = lst;
        }

        c = next_char(port);
        if (closed_paren(c))        // end of list
        {
            update_port(port, c);
            break;
        }
        else if (c == port_eof(port))
        {
            if (!(flags & READ_FLAG_WAIT))
            {
                *perr = end_of_input_error();
                break;
            }

            printf(" ");
        }
        else
        {
            put_back(port, c);
        }
    }

    END_SYNTAX_LOC(loc, port);
    lst = minim_ast(lst, loc);
    expand_list(lst, perr);
    CHECK_EOF(port, c);
    return lst;
}

static MinimObject *read_vector(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *vec, *el;
    SyntaxLoc *loc;
    char c;

    START_SYNTAX_LOC(loc, port);
    vec = minim_vector(0, NULL);
    c = next_char(port);
    if (c == ')')           // empty list
    {
        update_port(port, c);
        END_SYNTAX_LOC(loc, port);
        return minim_ast(vec, loc);
    }

    put_back(port, c);
    while (true)
    {
        el = read_top(port, perr, flags);
        if (*perr)      // parse error
        {
            END_SYNTAX_LOC(loc, port);
            return minim_ast(vec, loc);
        }

        MINIM_VECTOR_RESIZE(vec, MINIM_VECTOR_LEN(vec) + 1);
        MINIM_VECTOR_REF(vec, MINIM_VECTOR_LEN(vec) - 1) = el;
        c = next_char(port);
        if (closed_paren(c))        // end of list
        {
            update_port(port, c);
            break;
        }
        else if (c == port_eof(port))
        {
            if (!(flags & READ_FLAG_WAIT))
            {
                *perr = end_of_input_error();
                break;
            }

            printf(" ");
        }
        else
        {
            put_back(port, c);
        }
    }

    END_SYNTAX_LOC(loc, port);
    vec = minim_ast(vec, loc);
    CHECK_EOF(port, c);
    return vec;
}

static MinimObject *read_datum(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *obj;
    SyntaxLoc *loc;
    Buffer *bf;
    char c;

    START_SYNTAX_LOC(loc, port);
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
    END_SYNTAX_LOC(loc, port);
    if (is_rational(get_buffer(bf)))
    {
        obj = str_to_number(get_buffer(bf), MINIM_OBJ_EXACT);
    }
    else if (is_float(get_buffer(bf)))
    {
        obj = str_to_number(get_buffer(bf), MINIM_OBJ_INEXACT);
    }
    else
    {    // symbol
        obj = intern(get_buffer(bf));
    }

    return minim_ast(obj, loc);
}

static MinimObject *read_string(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    SyntaxLoc *loc;
    Buffer *bf;
    char c;

    START_SYNTAX_LOC(loc, port);
    init_buffer(&bf);

    c = next_ch(port);
    update_port(port, c);

    while (c != '"')
    {
        if (c == port_eof(port))
        {
            *perr = end_of_input_error();
            END_SYNTAX_LOC(loc, port);
            return minim_ast(minim_string(get_buffer(bf)), loc);
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

    trim_buffer(bf);
    END_SYNTAX_LOC(loc, port);
    return minim_ast(intern_string(global.strings, get_buffer(bf)), loc);
}

static MinimObject *read_char(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *ch;
    SyntaxLoc *loc;
    Buffer *bf;
    char c;

    START_SYNTAX_LOC(loc, port);
    init_buffer(&bf);
    writes_buffer(bf, "#\\");
    c = next_ch(port);
    update_port(port, c);

    // if (c == 'u')            unicode scalar
    // {
    //     c = fgetc(file);
    // }
    // else
    // {
    writec_buffer(bf, c);
    // }

    trim_buffer(bf);
    END_SYNTAX_LOC(loc, port);
    ch = minim_char(c);
    return minim_ast(ch, loc);
}

static MinimObject *read_top(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *node;
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
            node = read_vector(port, perr, flags);

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
                *perr = bad_syntax_error(MINIM_STX_SYMBOL(node));
        }
    }
    else if (open_paren(c))
    {
        update_port(port, c);
        node = read_list(port, perr, flags);
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
            node = NULL;
        }
    }
    else if (normal_char(c))
    {
        put_back(port, c);
        node = read_datum(port, perr, flags);
    }
    else
    {
        update_port(port, c);
        *perr = unexpected_char_error(c);
        node = NULL;
    }

    return node;
}

//
// Exported
//

MinimObject *minim_parse_port(MinimObject *port, MinimObject **perr, uint8_t flags)
{
    MinimObject *stx;
    char c;

    *perr = NULL;
    stx = read_top(port, perr, flags);
    if (*perr == NULL && MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        CHECK_EOF(port, c);
    return stx;
}

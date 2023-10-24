// read.c: minimal Scheme reader

#include "minim.h"

// special characters

#define NUL_CHAR        ((char) 0x00)       // null
#define BEL_CHAR        ((char) 0x07)       // alarm / bell
#define BS_CHAR         ((char) 0x08)       // backspace
#define HT_CHAR         ((char) 0x09)       // horizontal tab
#define LF_CHAR         ((char) 0x0A)       // line feed
#define VT_CHAR         ((char) 0x0B)       // vertical tab
#define FF_CHAR         ((char) 0x0B)       // form feed (page break)
#define CR_CHAR         ((char) 0x0D)       // carriage return
#define ESC_CHAR        ((char) 0x1B)       // escape
#define SP_CHAR         ((char) 0x20)       // space
#define DEL_CHAR        ((char) 0x7F)       // delete

// port shims

#define ip_getc(ip)         (getc(minim_port(ip)))
#define ip_ungetc(ip, c)    (ungetc(c, minim_port(ip)))

static int ip_peek(mobj ip) {
    int c = ip_getc(ip);
    ip_ungetc(ip, c);
    return c;
}

// Forces the start of a new token
static int is_delimeter(mchar c) {
    return isspace(c) || c == EOF || 
           c == '(' || c == ')' ||
           c == '[' || c == ']' ||
           c == '{' || c == '}' ||
           c == '"' || c == ';';
}

static int is_open_paren(mchar c) {
    return c == '(' || c == '[' || c == '{';
}

static int is_closed_paren(mchar c) {
    return c == ')' || c == ']' || c == '}';
}

// Anything that doesn't force a new token
static int is_symbol_char(mchar c) {
    return !is_delimeter(c);
}

static void assert_not_eof(mchar c) {
    if (c == EOF) {
        error("read_object()", "unexpected EOF");
    }
}

static void assert_matching_paren(mchar open, mchar closed) {
    if ((open == '(' && closed != ')') ||
        (open == '[' && closed != ']') ||
        (open == '{' && closed != '}')) {
        error("read_object()", "parenthesis mismatch");
    }
}

static void assert_delimeter_next(mobj ip) {
    if (!is_delimeter(ip_peek(ip))) {
        error("read_object()", "expected a delimeter");
    }
}

// Continually reads from the input port until
// a non-whitespace character is found.
static void skip_whitespace(mobj ip) {
    int c;
    while ((c = ip_getc(ip)) != EOF) {
        if (isspace(c)) {
            // whitespace
            continue;
        } else if (c == ';') {
            // comment: ignore until newline
            while (((c = ip_getc(ip)) != EOF) && (c != '\n'));
            ip_ungetc(ip, c);
        }

        // too far
        ip_ungetc(ip, c);
        break;
    }
}

// Skips whitespace and returns the next character.
// If an EOF occurs, panics.
static mchar next_non_whitespace(mobj ip) {
    skip_whitespace(ip);
    mchar c = ip_getc(ip);
    assert_not_eof(c);
    return c;
}

// Continually reads from the input port while parsing
// a block comment, until the block is exited.
static void skip_block_comment(mobj ip) {
    size_t block_level = 1;
    while (block_level > 0) {
        mchar c = ip_getc(ip);
        assert_not_eof(c);
        if (c == '#') {
            c = ip_peek(ip);
            if (c == '|') {
                // increase block level
                ip_getc(ip);
                ++block_level;
            }
        } else if (c == '|') {
            c = ip_peek(ip);
            if (c == '#') {
                // decrease block level
                ip_getc(ip);
                --block_level;
            }
        }
    }
}

static void read_named_char(mobj ip, const char *s) {
    mchar c;
    while (*s != '\0') {
        c = ip_getc(ip);
        if (c != *s) {
            error1("read_object()",
                   "unexpected character when parsing named char",
                   Mchar(c));
        }
    }
}

// Reads a character from the input port.
// Handles the following named characters:
// - nul, alarm, backspace, tab, linefeed, newline,
//   vtab, page, return, esc, space, delete
static mobj read_char(mobj ip) {
    mchar c, nc;

    c = ip_getc(ip);
    if (c == EOF) {
        error("read_object()", "unexpected EOF while parsing character sequence");
    } else if (c == 'a') {
        nc = ip_peek(ip);
        if (nc == 'l') {
            // alarm
            read_named_char(ip, "larm");
            assert_delimeter_next(ip);
            return Mchar(BEL_CHAR);
        }
    } else if (c == 'b') {
        nc = ip_peek(ip);
        if (nc == 'a') {
            // backspace
            read_named_char(ip, "ackspace");
            assert_delimeter_next(ip);
            return Mchar(BS_CHAR);
        }
    } else if (c == 'd') {
        nc = ip_peek(ip);
        if (nc == 'e') {
            // delete
            read_named_char(ip, "elete");
            assert_delimeter_next(ip);
            return Mchar(DEL_CHAR);
        }
    } else if (c == 'e') {
        nc = ip_peek(ip);
        if (nc == 's') {
            // esc
            read_named_char(ip, "sc");
            assert_delimeter_next(ip);
            return Mchar(ESC_CHAR);
        }
    } else if (c == 'l') {
        nc = ip_peek(ip);
        if (nc == 'i') {
            // linefeed
            read_named_char(ip, "inefeed");
            assert_delimeter_next(ip);
            return Mchar(LF_CHAR);
        }
    } else if (c == 'n') {
        nc = ip_peek(ip);
        if (nc == 'e') {
            // newline
            read_named_char(ip, "ewline");
            assert_delimeter_next(ip);
            return Mchar('\n');
        } else if (nc == 'u') {
            // nul
            read_named_char(ip, "ul");
            assert_delimeter_next(ip);
            return Mchar('\0');
        }
    } else if (c == 'p') {
        nc = ip_peek(ip);
        if (nc == 'a') {
            // page
            read_named_char(ip, "age");
            assert_delimeter_next(ip);
            return Mchar(FF_CHAR);
        }
    } else if (c == 'r') {
        nc = ip_peek(ip);
        if (nc == 'e') {
            // return
            read_named_char(ip, "eturn");
            assert_delimeter_next(ip);
            return Mchar(CR_CHAR);
        }
    } else if (c == 's') {
        nc = ip_peek(ip);
        if (nc == 'p') {
            // space
            read_named_char(ip, "pace");
            assert_delimeter_next(ip);
            return Mchar(' ');
        }
    } else if (c == 't') {
        nc = ip_peek(ip);
        if (nc == 'a') {
            // tab
            read_named_char(ip, "ab");
            assert_delimeter_next(ip);
            return Mchar(HT_CHAR);
        }
    } else if (c == 'v') {
        nc = ip_peek(ip);
        if (nc == 't') {
            // vtab
            read_named_char(ip, "tab");
            assert_delimeter_next(ip);
            return Mchar(VT_CHAR);
        }
    }

    return Mchar(c);
}

// Pair/list reader
static mobj read_pair(mobj ip, mchar open_paren) {
    mobj car, cdr;
    mchar c;

    // check for empty list
    c = next_non_whitespace(ip);
    if (is_closed_paren(c)) {
        assert_matching_paren(open_paren, c);
        return minim_null;
    }

    // read in car element
    ip_ungetc(ip, c);
    car = read_object(ip);

    // check for cons dot
    c = next_non_whitespace(ip);
    if (c == '.' && isspace(ip_peek(ip))) {
        // either a pair or an improper list
        // read in the cdr element
        assert_delimeter_next(ip);
        cdr = read_object(ip);

        // must be terminated by closing paren
        c = next_non_whitespace(ip);
        if (!is_closed_paren(c)) {
            error1("read_object()", "missing pair terminator", Mchar(c));
        }

        assert_matching_paren(open_paren, c);
    } else {
        // (improper) list
        ip_ungetc(ip, c);
        cdr = read_pair(ip, open_paren);
    }

    return Mcons(car, cdr);
}

// Vector reader
static mobj read_vector(mobj ip, mchar open_paren) {
    mobj l;
    mchar c;

    skip_whitespace(ip);
    c = ip_peek(ip);
    if (is_closed_paren(c)) {
        ip_getc(ip);
        assert_matching_paren(open_paren, c);
        return M_glob.null_vector;
    }

    l = minim_null;
    while (1) {
        l = Mcons(read_object(ip), l);
        skip_whitespace(ip);
        c = ip_peek(ip);
        if (is_closed_paren(c)) {
            ip_getc(ip);
            assert_matching_paren(open_paren, c);
            break;
        }
    }

    return list_to_vector(list_reverse(l));  
}

// Symbol reader with the first character
mobj read_symbol(mobj ip, mchar c) {
    mchar *buffer;
    size_t len, i;

    // set up the buffer
    len = INIT_READ_BUFFER_LEN;
    buffer = GC_alloc_atomic(len * sizeof(mchar));
    
    // set the first character
    buffer[0] = c;
    i = 1;

    // iteratively read
    for (c = ip_getc(ip); is_symbol_char(c); c = ip_getc(ip)) {
        // add the character
        buffer[i] = c;
        i++;

        // ensure enough space for the NULL character
        if (i + 1 >= len) {
            len *= 2;
            buffer = GC_realloc(buffer, len * sizeof(mchar));
        }
    }

    ip_ungetc(ip, c);
    buffer[i] = '\0';
    return intern_str(buffer);
}

// Top-level reader, invoked for reading arbitrary Scheme expressions.
mobj read_object(mobj ip) {
    mchar c;

loop:

    skip_whitespace(ip);
    c = ip_getc(ip);

    if (c == '#') {
        // special value: `#<c>...`
        c = ip_getc(ip);
        if (c == 't') {
            // #t => true
            return minim_true;
        } else if (c == 'f') {
            // #f => false
            return minim_false;
        } else if (c == '\\') {
            // #\<c>... => character
            return read_char(ip);
        } else if (is_open_paren(c)) {
            // #(...) => vector
            return read_vector(ip, c);
        } else if (c == '&') {
            // #&... => box
            return Mbox(read_object(ip));
        } else if (c == ';') {
            // #; => (datum comment, skip next datum)
            skip_whitespace(ip);
            c = ip_peek(ip);
            if (c != EOF) {
                read_object(ip);    // skip
                goto loop;
            }
            error("read_object()", "unexpected EOF while parsing datum comment");
        } else if (c == '|') {
            // #| => (block comment)
            skip_block_comment(ip);
            goto loop;
        } else {
            error1("read_object()",
                   "unexpected character following `#`",
                   Mchar(c));
        }
    } else if (is_open_paren(c)) {
        // pair of list
        return read_pair(ip, c);
    } else if (is_symbol_char(c)) {
        // symbols
        return read_symbol(ip, c);
    } else if (c == EOF) {
        // EOF
        return minim_eof;
    } else {
        // unknown
        error1("read_object()", "unexpected input", Mchar(c));
    }
}

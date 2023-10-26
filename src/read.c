// read.c: minimal Scheme reader

#include "minim.h"

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

static int is_decnum_str(const mchar *s) {
    size_t i;
    int p;

    // optional sign
    if (s[0] == '\n') {
        return 0;
    } else if (s[0] == '+' || s[0] == '-') {
        i = 1;
        p = 1;
    } else {
        i = 0;
        p = 0;
    }

    // scan through decimal characters
    for (; s[i] && isdigit(s[i]); i++); 

    // different valid conditions depending on sign
    if (p) {
        return i >= 2 && !s[i];
    } else {
        return i >= 1 && !s[i];
    }
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
    for (; *s != '\0'; s++) {
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

// Hexadecimal number reader.
// If an invalid character is read, panic.
static mobj read_hexnum(mobj ip) {
    mfixnum m = 0;
    int s = 1;
    mchar c;

    // optional sign
    c = ip_peek(ip);
    if (c == '-') {
        ip_getc(ip);
        s = -1;
    } else if (c == '+') {
        ip_getc(ip);
    }

    // magnitude as hex digits
    for (c = ip_getc(ip); isxdigit(c); c = ip_getc(ip)) {
        m *= 16;
        if ('A' <= c && c <= 'F') {
            m += (10 + (c - 'A'));
        } else if ('a' <= c && c <= 'f') {
            m += (10 + (c - 'a'));
        } else {
            m += (c - '0');
        }
    }

    // check if we read an illegal character
    ip_ungetc(ip, c);
    if (is_symbol_char(c)) {
        error1("read_object()", "unexpected character while parsing hexadecimal number", Mchar(c));
    }

    // check for delimeter
    assert_delimeter_next(ip);
    return Mfixnum(s * m);
}

// String to decnum converter.
static mobj str_to_decnum(const mchar *str) {
    mfixnum m;
    int s, i;
    
    // optional sign
    if (str[0] == '-') {
        s = -1;
        i = 1;
    } else if (str[0] == '+') {
        s = 1;
        i = 1;
    } else {
        s = 1;
        i = 0;
    }

    // convert magnitude as decimal digits
    m = 0;
    for (; str[i]; ++i) {
        m *= 10;
        m += (str[i] - '0');
    }

    return Mfixnum(s * m);
}

// Pair/list reader.
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
    if (c == '.' && is_delimeter(ip_peek(ip))) {
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

// String character (opening quote has been read)
mobj read_string(mobj ip) {
    mchar *buffer;
    size_t len, i;
    mchar c;

    // set up the buffer
    len = INIT_READ_BUFFER_LEN;
    buffer = GC_alloc_atomic(len * sizeof(mchar));
    i = 0;

    // iteratively read
    for (c = ip_getc(ip); c != '\"'; c = ip_getc(ip)) {
        if (c == EOF) {
            error("read_object()", "unexpected EOF while parsing string");
        } else if (c == '\\') {
            // escape character
            c = ip_getc(ip);
            if (c == EOF) {
                error("read_object()", "unexpected EOF while parsing escape character");
            } else if (c == 'a') {
                // alarm
                buffer[i] = BEL_CHAR;
            } else if (c == 'b') {
                // backspace
                buffer[i] = BS_CHAR;
            } else if (c == 't') {
                // horizontal tab
                buffer[i] = '\t';
            } else if (c == 'n') {
                // newline
                buffer[i] = '\n';
            } else if (c == 'v') {
                // vertical tab
                buffer[i] = VT_CHAR;
            } else if (c == 'f') {
                // form feed
                buffer[i] = FF_CHAR;
            } else if (c == 'r') {
                // carriage return
                buffer[i] = CR_CHAR;
            } else if (c == '\"') {
                // double quote
                buffer[i] = '\"';
            } else if (c == '\\') {
                // backslash
                buffer[i] = '\\';
            } else {
                error1("read_object()", "unknown escape character", Mchar(c));
            }
        } else {
            // normal character
            buffer[i] = c;
        }

        i++;

        // ensure enough space for the NULL character
        if (i + 1 >= len) {
            len *= 2;
            buffer = GC_realloc(buffer, len * sizeof(mchar));
        }
    }

    buffer[i] = '\0';
    return Mstring2(buffer);
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

    // we could have read an integer
    if (is_decnum_str(buffer)) {
        return str_to_decnum(buffer);
    } else {
        return intern_str(buffer);
    }
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
        } else if (c == 'x') {
            // #x... => (hexadecimal number)
            return read_hexnum(ip);
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
    } else if (c == '\"') {
        // string
        return read_string(ip);
    } else if (c == '\'') {
        // shorthand for quote
        return Mlist2(quote_sym, read_object(ip));
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

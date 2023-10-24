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

static int peek_char(mobj ip) {
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
    if (!is_delimeter(peek_char(ip))) {
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

static void read_expected_charname(mobj ip, const char *s) {
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
        nc = peek_char(ip);
        if (nc == 'l') {
            // alarm
            read_expected_charname(ip, "larm");
            assert_delimeter_next(ip);
            return Mchar(BEL_CHAR);
        }
    } else if (c == 'b') {
        nc = peek_char(ip);
        if (nc == 'a') {
            // backspace
            read_expected_charname(ip, "ackspace");
            assert_delimeter_next(ip);
            return Mchar(BS_CHAR);
        }
    } else if (c == 'd') {
        nc = peek_char(ip);
        if (nc == 'e') {
            // delete
            read_expected_charname(ip, "elete");
            assert_delimeter_next(ip);
            return Mchar(DEL_CHAR);
        }
    } else if (c == 'e') {
        nc = peek_char(ip);
        if (nc == 's') {
            // esc
            read_expected_charname(ip, "sc");
            assert_delimeter_next(ip);
            return Mchar(ESC_CHAR);
        }
    } else if (c == 'l') {
        nc = peek_char(ip);
        if (nc == 'i') {
            // linefeed
            read_expected_charname(ip, "inefeed");
            assert_delimeter_next(ip);
            return Mchar(LF_CHAR);
        }
    } else if (c == 'n') {
        nc = peek_char(ip);
        if (nc == 'e') {
            // newline
            read_expected_charname(ip, "ewline");
            assert_delimeter_next(ip);
            return Mchar('\n');
        } else if (nc == 'u') {
            // nul
            read_expected_charname(ip, "ul");
            assert_delimeter_next(ip);
            return Mchar('\0');
        }
    } else if (c == 'p') {
        nc = peek_char(ip);
        if (nc == 'a') {
            // page
            read_expected_charname(ip, "age");
            assert_delimeter_next(ip);
            return Mchar(FF_CHAR);
        }
    } else if (c == 'r') {
        nc = peek_char(ip);
        if (nc == 'e') {
            // return
            read_expected_charname(ip, "eturn");
            assert_delimeter_next(ip);
            return Mchar(CR_CHAR);
        }
    } else if (c == 's') {
        nc = peek_char(ip);
        if (nc == 'p') {
            // space
            read_expected_charname(ip, "pace");
            assert_delimeter_next(ip);
            return Mchar(' ');
        }
    } else if (c == 't') {
        nc = peek_char(ip);
        if (nc == 'a') {
            // tab
            read_expected_charname(ip, "ab");
            assert_delimeter_next(ip);
            return Mchar(HT_CHAR);
        }
    } else if (c == 'v') {
        nc = peek_char(ip);
        if (nc == 't') {
            // vtab
            read_expected_charname(ip, "tab");
            assert_delimeter_next(ip);
            return Mchar(VT_CHAR);
        }
    }

    return Mchar(c);
}

// Vector reader
static mobj read_vector(mobj ip) {
    mobj l;
    mchar c;

    skip_whitespace(ip);
    c = peek_char(ip);
    if (c == ')') {
        read_char(ip);
        return M_glob.null_vector;
    }

    l = minim_null;
    while (1) {
        l = Mcons(read_object(ip), l);
        skip_whitespace(ip);
        c = peek_char(ip);
        if (c == ')') {
            read_char(ip);
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
    while (is_symbol_char(c = ip_getc(ip))) {
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
        } else if (c == '(') {
            // #(...) => vector
            return read_vector(ip);
        } else if (c == '&') {
            // #&... => box
            return Mbox(read_object(ip));
        } else if (c == ';') {
            // #; => (datum comment, skip next datum)
            skip_whitespace(ip);
            c = peek_char(ip);
            if (c != EOF) {
                read_object(ip);    // skip
                goto loop;
            }

            error("read_object()", "unexpected EOF while parsing datum comment");
        } else {
            error1("read_object()",
                   "unexpected character following `#`",
                   Mchar(c));
        }
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

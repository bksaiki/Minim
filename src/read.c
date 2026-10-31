// read.c: reader

#include "minim.h"

static int delimeterp(int c) {
    return isspace(c)
        || c == EOF
        || c == '('
        || c == ')'
        || c == '['
        || c == ']'
        || c == '{'
        || c == '}'
        || c == '"'
        || c == ';';
}

static int symbol_charp(int c) {
    return !delimeterp(c);
}

static void assert_not_eof(char c) {
    if (c == EOF) {
        minim_error("read_object", "unexpected end of input");
    }
}

static void assert_delimeter(char c) {
    if (!delimeterp(c)) {
        minim_error1("read_object", "expected delimeter", Mchar(c));
    }
}

static void assert_matching_paren(char open, char closed) {
    if (!(open == '(' && closed == ')') && 
        !(open == '[' && closed == ']') &&
        !(open == '{' && closed == '}')) {
        minim_error2("read_object", "parenthesis mismatch", Mchar(closed), Mchar(open));
    }
}

#define assert_next_delimeter(ip) \
    assert_delimeter(port_peek(ip))

static void skip_whitespace(obj ip) {
    int c;
    while ((c = port_peek(ip)) != EOF) {
        if (isspace(c)) {
            // whitespace => consume the character
            port_read(ip);
            continue;
        } else if (c == ';') {
            // comment => consume until newline
            port_read(ip);
            for (c = port_peek(ip); c != '\n'; c = port_peek(ip))
                port_read(ip);
            port_read(ip);
            continue;
        }

        break;
    }
}

static iptr char_to_decnum(int c) {
    return c - '0';
}

static iptr char_to_hexnum(int c) {
    if ('A' <= c && c <= 'F')
        return 10  + (c - 'A');
    else if ('a' <= c && c <= 'f')
        return 10  + (c - 'a');
    else
        return char_to_decnum(c);
}

static void read_named_char(obj ip, const char *s) {
    int c;
    for (; *s != '\0'; s++) {
        c = port_read(ip);
        if (c != *s) {
            minim_error1("read_object", "unexpected character when parsing named character", Mchar(c));
        }
    }
}

static obj read_char(obj ip) {
    int c, nc;

    c = port_read(ip);
    assert_not_eof(c);
    if (c == 'a') {
        nc = port_peek(ip);
        if (nc == 'l') {
            // alarm
            read_named_char(ip, "larm");
            assert_next_delimeter(ip);
            return Mchar(BEL_CHAR);
        }
    } else if (c == 'b') {
        nc = port_peek(ip);
        if (nc == 'a') {
            // backspace
            read_named_char(ip, "ackspace");
            assert_next_delimeter(ip);
            return Mchar(BS_CHAR);
        }
    } else if (c == 'd') {
        nc = port_peek(ip);
        if (nc == 'e') {
            // delete
            read_named_char(ip, "elete");
            assert_next_delimeter(ip);
            return Mchar(DEL_CHAR);
        }
    } else if (c == 'e') {
        nc = port_peek(ip);
        if (nc == 's') {
            // esc
            read_named_char(ip, "sc");
            assert_next_delimeter(ip);
            return Mchar(ESC_CHAR);
        }
    } else if (c == 'l') {
        nc = port_peek(ip);
        if (nc == 'i') {
            // linefeed
            read_named_char(ip, "inefeed");
            assert_next_delimeter(ip);
            return Mchar(LF_CHAR);
        }
    } else if (c == 'n') {
        nc = port_peek(ip);
        if (nc == 'e') {
            // newline
            read_named_char(ip, "ewline");
            assert_next_delimeter(ip);
            return Mchar('\n');
        } else if (nc == 'u') {
            // nul
            read_named_char(ip, "ul");
            assert_next_delimeter(ip);
            return Mchar('\0');
        }
    } else if (c == 'p') {
        nc = port_peek(ip);
        if (nc == 'a') {
            // page
            read_named_char(ip, "age");
            assert_next_delimeter(ip);
            return Mchar(FF_CHAR);
        }
    } else if (c == 'r') {
        nc = port_peek(ip);
        if (nc == 'e') {
            // return
            read_named_char(ip, "eturn");
            assert_next_delimeter(ip);
            return Mchar(CR_CHAR);
        }
    } else if (c == 's') {
        nc = port_peek(ip);
        if (nc == 'p') {
            // space
            read_named_char(ip, "pace");
            assert_next_delimeter(ip);
            return Mchar(' ');
        }
    } else if (c == 't') {
        nc = port_peek(ip);
        if (nc == 'a') {
            // tab
            read_named_char(ip, "ab");
            assert_next_delimeter(ip);
            return Mchar(HT_CHAR);
        }
    } else if (c == 'v') {
        nc = port_peek(ip);
        if (nc == 't') {
            // vtab
            read_named_char(ip, "tab");
            assert_next_delimeter(ip);
            return Mchar(VT_CHAR);
        }
    }

    assert_next_delimeter(ip);
    return Mchar(c);
}

static obj read_pair(obj ip, char open_paren) {
    obj car, cdr;
    int c;

    skip_whitespace(ip);
    c = port_peek(ip);
    assert_not_eof(c);

    if (c == ')' || c == ']' || c == '}') {
        // empty list
        port_read(ip);
        assert_matching_paren(open_paren, c);
        return Mnull;
    }

    car = read_object(ip);

    skip_whitespace(ip);
    c = port_peek(ip);
    assert_not_eof(c);

    if (c == '.' && port_read(ip) && delimeterp(c = port_peek(ip))) {
        // improper list
        assert_not_eof(c);
        port_read(ip);
        cdr = read_object(ip);

        skip_whitespace(ip);
        c = port_read(ip);
        assert_not_eof(c);

       if (c == ')' || c == ']' || c == '}') {
            // list read
            assert_matching_paren(open_paren, c);
            return Mcons(car, cdr);
        }

        minim_error1("read_object", "expected pair terminator", Mchar(c));      
    } else {
        // list
        cdr = read_pair(ip, open_paren);
        return Mcons(car, cdr);
    }
}

static obj read_vector(obj ip) {
    obj hd, tl, x;
    int c;

    hd = tl = Mnull;
    while (1) {
        skip_whitespace(ip);
        c = port_peek(ip);
        if (c == ')') {
            read_char(ip);
            break;
        }

        x = read_object(ip);
        if (Mnullp(hd)) {
            hd = tl = Mlist1(x);
        } else {
            Mcdr(tl) = Mlist1(x);
            tl = Mcdr(tl);
        }
    }

    return list_to_vector(hd);
}

obj read_object(obj ip) {
    char buffer[SYMBOL_MAX_LENGTH];
    long num, i, block_level;
    short sign;
    char c;

loop:

    skip_whitespace(ip);
    c = port_read(ip);
    if (c == '#') {
        // special value
        c = port_peek(ip);
        switch (c) {
        case '%':
            // symbol
            i = 1;
            buffer[0] = '#';
            goto read_symbol;
        case 't':
            // true
            port_read(ip);
            return Mtrue;
        case 'f':
            // false
            port_read(ip);
            return Mfalse;
        case '\\':
            // character
            port_read(ip);
            return read_char(ip);
        // case '\'':
        //     // quote
        //     return Mcons(intern("quote-syntax"), Mcons(read_object(in), minim_null));
        case '(':
            // vector
            port_read(ip);
            return read_vector(ip);
        // case '&':
        //     // vector
        //     return Mbox(read_object(in));
        case 'x':
            // hex number
            port_read(ip);
            c = port_read(ip);
            buffer[0] = '#';
            buffer[1] = 'x';
            goto read_hex;
        case ';':
            // datum comment
            port_read(ip);
            skip_whitespace(ip);
            c = port_peek(ip);
            assert_not_eof(c);
            read_object(ip);        // throw away
            goto loop;
        case '|':
            // block comment
            block_level = 1;
            while (block_level > 0) {
                c = port_read(ip);
                assert_not_eof(c);
                switch (c)
                {
                case '#':
                    c = port_read(ip);
                    assert_not_eof(c);
                    if (c == '|') ++block_level;
                    break;
                
                case '|':
                    c = port_read(ip);
                    assert_not_eof(c);
                    if (c == '#') --block_level;
                    break;
                }
            }

            goto loop;
        default:
            minim_error1("read_object", "unknown special datum", Mchar(c));   
        }
    } else if (isdigit(c) || ((c == '-' || c == '+') && isdigit(port_peek(ip)))) {
        // decimal number (possibly)
        // if we encounter a non-digit, the token is a symbol
        num = 0;
        sign = 1;
        i = 0;

        // optional sign
        buffer[i++] = c;
        if (c == '-') {
            sign = -1;
        } else if (c != '+') {
            num = char_to_decnum(c);
        }

        // magnitude
        while (isdigit(c = port_peek(ip))) {
            port_read(ip);
            num = (num * 10) + char_to_decnum(c);
            buffer[i++] = c;
        }

        if (symbol_charp(c)) {
            goto read_symbol;
        }

        // check for delimeter and construct the flonum
        assert_delimeter(c);
        return Mfixnum(num * sign);
    } else if (0) {
        // hexadecimal number
        // same caveat applies: if we encounter a non-digit, the token is a symbol
read_hex:
        num = 0;
        sign = 1;
        i = 0;

        // optional sign
        buffer[i++] = c;
        if (c == '-') {
            sign = -1;
        } else if (c != '+') {
            num = char_to_hexnum(c);
        }

        // magnitude
        while (isxdigit(c = port_peek(ip))) {
            port_read(ip);
            buffer[i++] = c;
            num = (num * 16) + char_to_hexnum(c);
        }

        if (symbol_charp(c)) {
            goto read_symbol;
        }

        // check for delimeter and construct the flonum
        assert_delimeter(c);
        return Mfixnum(num * sign);
    } else if (c == '"') {
        // string
        i = 0;
        while ((c = port_read(ip)) != '"') {
            assert_not_eof(c);
            if (c == '\\') {
                c = port_read(ip);
                if (c == 'n') {
                    c = '\n';
                } else if (c == 't') {
                    c = '\t';
                } else if (c == '\\') {
                    c = '\\';
                } else if (c == '\'') {
                    c = '\'';
                } else if (c == '\"') {
                    c = '\"';
                } else {
                    minim_error1("read_object", "unknown escape character", Mchar(c));
                }
            }

            if (i >= SYMBOL_MAX_LENGTH) {
                minim_error("read_object", "string exceeded max length");
            }

            buffer[i++] = c;
        }

        buffer[i] = '\0';
        return Mstring(buffer);
    } else if (c == '(' || c == '[' || c == '{') {
        // empty list or pair
        return read_pair(ip, c);
    } else if (c == '\'') {
        // quoted expression
        return Mcons(Mquote_symbol, Mcons(read_object(ip), Mnull));
    } else if (symbol_charp(c) || ((c == '+' || c == '-') && delimeterp(port_peek(ip)))) {
        // symbol
        i = 1;
        buffer[0] = c;

read_symbol:

        while (symbol_charp(c = port_peek(ip))) {
            if (i >= SYMBOL_MAX_LENGTH) {
                minim_error("read_object", "symbol exceeded max length");
            }

            port_read(ip);
            buffer[i++] = c;
        }

        assert_delimeter(c);
        buffer[i] = '\0';
        return Mintern(buffer);
    } else if (c == EOF) {
        return Meof;
    } else {
        minim_error1("read_object", "unexpected input", Mchar(c));
    }

    minim_error("read_object", "unreachable");
}

// read.c: reader

#include "../minim.h"

static void assert_not_eof(char c) {
    if (c == EOF) {
        fprintf(stderr, "unexpected end of input\n");
        minim_shutdown(1);
    }
}

static void assert_matching_paren(char open, char closed) {
    if (!(open == '(' && closed == ')') && 
        !(open == '[' && closed == ']') &&
        !(open == '{' && closed == '}')) {
        fprintf(stderr, "parenthesis mismatch: %c closed off %c\n", closed, open);
        minim_shutdown(1);
    }
}

static int is_delimeter(int c) {
    return isspace(c) || c == EOF || 
           c == '(' || c == ')' ||
           c == '[' || c == ']' ||
           c == '{' || c == '}' ||
           c == '"' || c == ';';
}

static int is_symbol_char(int c) {
    return !is_delimeter(c);
}

static int peek_char(FILE *in) {
    int c = getc(in);
    ungetc(c, in);
    return c;
}

static void peek_expected_delimeter(FILE *in) {
    if (!is_delimeter(peek_char(in))) {
        fprintf(stderr, "expected a delimeter\n");
        minim_shutdown(1);
    }
}

static int is_cons_dot(FILE *in, int c) {
    if (c != '.')
        return 0;

    // need to make sure it's actually a dot
    c = peek_char(in);
    return isspace(c);
}

static void skip_whitespace(FILE *in) {
    int c;

    while ((c = getc(in)) != EOF) {
        if (isspace(c))
            continue;
        else if (c == ';') {
            // comment: ignore until newline
            while (((c = getc(in)) != EOF) && (c != '\n'));
            ungetc(c, in);
            continue;
        }
        
        // too far
        ungetc(c, in);
        break;
    }
}

static void read_named_char(FILE *in, const char *s) {
    mchar c;
    for (; *s != '\0'; s++) {
        c = getc(in);
        if (c != *s) {
            fprintf(stderr, "unexpected character when parsing named char\n");
            minim_shutdown(1);
        }
    }
}

static mobj read_char(FILE *in) {
    int c, nc;

    c = getc(in);
    if (c == EOF) {
        fprintf(stderr, "unexpected EOF while parsing character sequence");
        minim_shutdown(1);
    } else if (c == 'a') {
        nc = peek_char(in);
        if (nc == 'l') {
            // alarm
            read_named_char(in, "larm");
            peek_expected_delimeter(in);
            return Mchar(BEL_CHAR);
        }
    } else if (c == 'b') {
        nc = peek_char(in);
        if (nc == 'a') {
            // backspace
            read_named_char(in, "ackspace");
            peek_expected_delimeter(in);
            return Mchar(BS_CHAR);
        }
    } else if (c == 'd') {
        nc = peek_char(in);
        if (nc == 'e') {
            // delete
            read_named_char(in, "elete");
            peek_expected_delimeter(in);
            return Mchar(DEL_CHAR);
        }
    } else if (c == 'e') {
        nc = peek_char(in);
        if (nc == 's') {
            // esc
            read_named_char(in, "sc");
            peek_expected_delimeter(in);
            return Mchar(ESC_CHAR);
        }
    } else if (c == 'l') {
        nc = peek_char(in);
        if (nc == 'i') {
            // linefeed
            read_named_char(in, "inefeed");
            peek_expected_delimeter(in);
            return Mchar(LF_CHAR);
        }
    } else if (c == 'n') {
        nc = peek_char(in);
        if (nc == 'e') {
            // newline
            read_named_char(in, "ewline");
            peek_expected_delimeter(in);
            return Mchar('\n');
        } else if (nc == 'u') {
            // nul
            read_named_char(in, "ul");
            peek_expected_delimeter(in);
            return Mchar('\0');
        }
    } else if (c == 'p') {
        nc = peek_char(in);
        if (nc == 'a') {
            // page
            read_named_char(in, "age");
            peek_expected_delimeter(in);
            return Mchar(FF_CHAR);
        }
    } else if (c == 'r') {
        nc = peek_char(in);
        if (nc == 'e') {
            // return
            read_named_char(in, "eturn");
            peek_expected_delimeter(in);
            return Mchar(CR_CHAR);
        }
    } else if (c == 's') {
        nc = peek_char(in);
        if (nc == 'p') {
            // space
            read_named_char(in, "pace");
            peek_expected_delimeter(in);
            return Mchar(' ');
        }
    } else if (c == 't') {
        nc = peek_char(in);
        if (nc == 'a') {
            // tab
            read_named_char(in, "ab");
            peek_expected_delimeter(in);
            return Mchar(HT_CHAR);
        }
    } else if (c == 'v') {
        nc = peek_char(in);
        if (nc == 't') {
            // vtab
            read_named_char(in, "tab");
            peek_expected_delimeter(in);
            return Mchar(VT_CHAR);
        }
    }

    peek_expected_delimeter(in);
    return Mchar(c);
}

static mobj read_pair(FILE *in, char open_paren) {
    mobj car, cdr;
    int c;

    skip_whitespace(in);
    c = getc(in);
    assert_not_eof(c);

    if (c == ')' || c == ']' || c == '}') {
        // empty list
        assert_matching_paren(open_paren, c);
        return minim_null;
    }

    ungetc(c, in);
    car = read_object(in);

    skip_whitespace(in);
    c = fgetc(in);
    assert_not_eof(c);

    if (is_cons_dot(in, c)) {
        // improper list
        peek_expected_delimeter(in);
        cdr = read_object(in);

        skip_whitespace(in);
        c = getc(in);
        assert_not_eof(c);

       if (c == ')' || c == ']' || c == '}') {
            // list read
            assert_matching_paren(open_paren, c);
            return Mcons(car, cdr);
        }

        fprintf(stderr, "missing ')' to terminate pair");
        minim_shutdown(1);        
    } else {
        // list
        ungetc(c, in);
        cdr = read_pair(in, open_paren);
        return Mcons(car, cdr);
    }
}

static mobj read_vector(FILE *in) {
    mobj l;
    int c;

    l = minim_null;
    while (1) {
        skip_whitespace(in);
        c = peek_char(in);
        if (c == ')') {
            read_char(in);
            break;
        }

        l = Mcons(read_object(in), l);
    }

    if (minim_nullp(l)) {
        return minim_empty_vec;
    } else {
        return list_to_vector(list_reverse(l));
    }
}

mobj read_object(FILE *in) {
    char buffer[SYMBOL_MAX_LEN];
    long num, i, block_level;
    short sign;
    char c;

loop:

    skip_whitespace(in);
    c = getc(in);
    // no assertion: may actually be at end
    // assert_not_eof(c);

    if (c == '#') {
        // special value
        c = getc(in);
        switch (c) {
        case 't':
            // true
            return minim_true;
        case 'f':
            // false
            return minim_false;
        case '\\':
            // character
            return read_char(in);
        case '\'':
            // quote
            return Mcons(intern("quote-syntax"), Mcons(read_object(in), minim_null));
        case '(':
            // vector
            return read_vector(in);
        case '&':
            // vector
            return Mbox(read_object(in));
        case 'x':
            // hex number
            c = getc(in);
            goto read_hex;
        case ';':
            // datum comment
            skip_whitespace(in);
            c = peek_char(in);
            assert_not_eof(c);
            read_object(in);        // throw away
            goto loop;
        case '|':
            // block comment
            block_level = 1;
            while (block_level > 0) {
                c = fgetc(in);
                assert_not_eof(c);
                switch (c)
                {
                case '#':
                    c = fgetc(in);
                    assert_not_eof(c);
                    if (c == '|') ++block_level;
                    break;
                
                case '|':
                    c = fgetc(in);
                    assert_not_eof(c);
                    if (c == '#') --block_level;
                    break;
                }
            }

            goto loop;
        default:
            fprintf(stderr, "unknown special constant");
            minim_shutdown(1);
        }
    } else if (isdigit(c) || ((c == '-' || c == '+') && isdigit(peek_char(in)))) {
        // decimal number (possibly)
        // if we encounter a non-digit, the token is a symbol
        num = 0;
        sign = 1;
        i = 0;

        // optional sign
        if (c == '-') {
            sign = -1;
            buffer[i++] = c;
        } else if (c != '+') {
            ungetc(c, in);
            buffer[i++] = c;
        }

        // magnitude
        while (isdigit(c = getc(in))) {
            buffer[i++] = c;
            num = (num * 10) + (c - '0');
        }

        if (is_symbol_char(c)) {
            goto read_symbol;
        }

        // check for delimeter
        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            minim_shutdown(1);
        }

        ungetc(c, in);
        return Mfixnum(num * sign);
    } else if (0) {
        // hexadecimal number
        // same caveat applies: if we encounter a non-digit, the token is a symbol
read_hex:
        num = 0;
        sign = 1;
        i = 0;

        // optional sign
        if (c == '-') {
            sign = -1;
            buffer[i++] = c;
        } else if (c != '+') {
            ungetc(c, in);
            buffer[i++] = c;
        }

        // magnitude
        while (isxdigit(c = getc(in))) {
            buffer[i++] = c;
            if ('A' <= c && c <= 'F')       num = (num * 16) + (10 + (c - 'A'));
            else if ('a' <= c && c <= 'f')  num = (num * 16) + (10 + (c - 'a'));
            else                            num = (num * 16) + (c - '0');
        }

        if (is_symbol_char(c)) {
            goto read_symbol;
        }

        // check for delimeter
        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            minim_shutdown(1);
        }

        ungetc(c, in);
        return Mfixnum(num * sign);
    } else if (c == '"') {
        // string
        i = 0;
        while ((c = getc(in)) != '"') {
            if (c == '\\') {
                c = getc(in);
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
                    fprintf(stderr, "unknown escape character: %c\n", c);
                    minim_shutdown(1);
                }
            } else if (c == EOF) {
                fprintf(stderr, "non-terminated string literal\n");
                minim_shutdown(1);
            }

            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "string is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                minim_shutdown(1);
            }
        }

        buffer[i] = '\0';
        return Mstring(buffer);
    } else if (c == '(' || c == '[' || c == '{') {
        // empty list or pair
        return read_pair(in, c);
    } else if (c == '\'') {
        // quoted expression
        return Mcons(intern("quote"), Mcons(read_object(in), minim_null));
    } else if (is_symbol_char(c) || ((c == '+' || c == '-') && is_delimeter(peek_char(in)))) {
        // symbol
        i = 0;

read_symbol:

        while (is_symbol_char(c)) {
            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "symbol is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                minim_shutdown(1);
            }
            c = getc(in);
        }

        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            minim_shutdown(1);
        }

        ungetc(c, in);
        buffer[i] = '\0';
        return intern(buffer);
    } else if (c == EOF) {
        return NULL;
    } else {
        fprintf(stderr, "unexpected input: %c\n", c);
        minim_shutdown(1);
    }

    fprintf(stderr, "unreachable");
    minim_shutdown(1);
}

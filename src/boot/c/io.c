/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.

    Basic file I/O.
*/

#include "boot.h"

//
//  Reading
//

static void assert_not_eof(char c) {
    if (c == EOF) {
        fprintf(stderr, "unexpected end of input\n");
        exit(1);
    }
}

static void assert_matching_paren(char open, char closed) {
    if (!(open == '(' && closed == ')') && 
        !(open == '[' && closed == ']') &&
        !(open == '{' && closed == '}')) {
        fprintf(stderr, "parenthesis mismatch: %c closed off %c\n", closed, open);
        exit(1);
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
    return c != '#' && !is_delimeter(c);
}

static int peek_char(FILE *in) {
    int c = getc(in);
    ungetc(c, in);
    return c;
}

static void peek_expected_delimeter(FILE *in) {
    if (!is_delimeter(peek_char(in))) {
        fprintf(stderr, "expected a delimeter\n");
        exit(1);
    }
}

static void read_expected_string(FILE *in, const char *s) {
    int c;

    while (*s != '\0') {
        c = getc(in);
        if (c != *s) {
            fprintf(stderr, "unexpected character: '%c'\n", c);
            exit(1);
        }
        ++s;
    }
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

static minim_object *read_char(FILE *in) {
    int c;

    c = getc(in);
    switch (c) {
    case EOF:
        fprintf(stderr, "incomplete character literal\n");
        break;
    case 's':
        if (peek_char(in) == 'p') {
            read_expected_string(in, "pace");
            peek_expected_delimeter(in);
            return make_char(' ');
        }
        break;
    case 'n':
        if (peek_char(in) == 'e') {
            read_expected_string(in, "ewline");
            peek_expected_delimeter(in);
            return make_char('\n');
        }
        break;
    default:
        break;
    }

    peek_expected_delimeter(in);
    return make_char(c);
}

static minim_object *read_pair(FILE *in, char open_paren) {
    minim_object *car, *cdr;
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

    if (c == '.') {
        // improper list
        peek_expected_delimeter(in);
        cdr = read_object(in);

        skip_whitespace(in);
        c = getc(in);
        assert_not_eof(c);

       if (c == ')' || c == ']' || c == '}') {
            // list read
            assert_matching_paren(open_paren, c);
            return make_pair(car, cdr);
        }

        fprintf(stderr, "missing ')' to terminate pair");
        exit(1);        
    } else {
        // list
        ungetc(c, in);
        cdr = read_pair(in, open_paren);
        return make_pair(car, cdr);
    }
}

minim_object *read_object(FILE *in) {
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
            return make_pair(intern("quote-syntax"), make_pair(read_object(in), minim_null));
        case ';':
            // datum comment
            skip_whitespace(in);
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
            exit(1);
        }
    } else if (isdigit(c) || ((c == '-' || c == '+') && isdigit(peek_char(in)))) {
        // number
        num = 0;
        sign = 1;

        // optional sign
        if (c == '-') {
            sign = -1;
        } else if (c != '+') {
            ungetc(c, in);
        }

        // magnitude
        while (isdigit(c = getc(in))) {
            num = (num * 10) + (c - '0');
        }

        // check for delimeter
        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            exit(1);
        }

        ungetc(c, in);
        return make_fixnum(num * sign);
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
                } else {
                    fprintf(stderr, "unknown escape character: %c\n", c);
                    exit(1);
                }
            } else if (c == EOF) {
                fprintf(stderr, "non-terminated string literal\n");
                exit(1);
            }

            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "string is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                exit(1);
            }
        }

        buffer[i] = '\0';
        return make_string(buffer);
    } else if (c == '(' || c == '[' || c == '{') {
        // empty list or pair
        return read_pair(in, c);
    } else if (c == '\'') {
        // quoted expression
        return make_pair(intern("quote"), make_pair(read_object(in), minim_null));
    } else if (is_symbol_char(c) || ((c == '+' || c == '-') && is_delimeter(peek_char(in)))) {
        // symbol
        i = 0;

        while (is_symbol_char(c) || isdigit(c) || c == '+' || c == '-') {
            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "symbol is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                exit(1);
            }
            c = getc(in);
        }

        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            exit(1);
        }

        ungetc(c, in);
        buffer[i] = '\0';
        return intern(buffer);
    } else if (c == EOF) {
        return NULL;
    } else {
        fprintf(stderr, "unexpected input: %c\n", c);
        exit(1);
    }

    fprintf(stderr, "unreachable");
    exit(1);
}

//
//  Writing
//

static void write_pair(FILE *out, minim_pair_object *p, int quote, int display) {
    write_object2(out, minim_car(p), quote, display);
    if (minim_is_pair(minim_cdr(p))) {
        fputc(' ', out);
        write_pair(out, ((minim_pair_object *) minim_cdr(p)), quote, display);
    } else if (minim_is_null(minim_cdr(p))) {
        return;
    } else {
        fprintf(out, " . ");
        write_object2(out, minim_cdr(p), quote, display);
    }
}

void write_object2(FILE *out, minim_object *o, int quote, int display) {
    char *str;

    switch (o->type) {
    case MINIM_NULL_TYPE:
        if (!quote) fputc('\'', out);
        fprintf(out, "()");
        break;
    case MINIM_TRUE_TYPE:
        fprintf(out, "#t");
        break;
    case MINIM_FALSE_TYPE:
        fprintf(out, "#f");
        break;
    case MINIM_EOF_TYPE:
        fprintf(out, "#<eof>");
        break;
    case MINIM_VOID_TYPE:
        fprintf(out, "#<void>");
        break;

    case MINIM_SYMBOL_TYPE:
        if (!quote) fputc('\'', out);
        fprintf(out, "%s", minim_symbol(o));
        break;
    case MINIM_FIXNUM_TYPE:
        fprintf(out, "%ld", minim_fixnum(o));
        break;
    case MINIM_CHAR_TYPE:
        if (display) {
            fputc(minim_char(o), out);
        } else {
            switch (minim_char(o)) {
            case '\n':
                fprintf(out, "#\\newline");
                break;
            case ' ':
                fprintf(out, "#\\space");
                break;
            default:
                fprintf(out, "#\\%c", minim_char(o));
            }
        }
        break;
    case MINIM_STRING_TYPE:
        str = minim_string(o);
        if (display) {
            fprintf(out, "%s", str);
        } else {
            fputc('"', out);
            while (*str != '\0') {
                switch (*str) {
                case '\n':
                    fprintf(out, "\\n");
                    break;
                case '\t':
                    fprintf(out, "\\t");
                    break;
                case '\\':
                    fprintf(out, "\\\\");
                    break;
                default:
                    fputc(*str, out);
                    break;
                }
                ++str;
            }
            fputc('"', out);
        }
        break;
    case MINIM_PAIR_TYPE:
        if (!quote) fputc('\'', out);
        fputc('(', out);
        write_pair(out, ((minim_pair_object *) o), 1, display);
        fputc(')', out);
        break;
    case MINIM_PRIM_PROC_TYPE:
        fprintf(out, "#<procedure:%s>", minim_prim_proc_name(o));
        break;
    case MINIM_CLOSURE_PROC_TYPE:
        if (minim_closure_name(o) != NULL)
            fprintf(out, "#<procedure:%s>", minim_closure_name(o));
        else
            fprintf(out, "#<procedure>");
        break;
    case MINIM_PORT_TYPE:
        if (minim_port_is_ro(o))
            fprintf(out, "#<input-port>");
        else
            fprintf(out, "#<output-port>");
        break;
    case MINIM_SYNTAX_TYPE:
        fprintf(out, "#<syntax ");
        write_object2(out, strip_syntax(o), 1, display);
        fputc('>', out);
        break;

    default:
        fprintf(stderr, "cannot write unknown object");
        exit(1);
    }
}

void write_object(FILE *out, minim_object *o) {
    write_object2(out, o, 0, 0);
}

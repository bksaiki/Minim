/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.

    Basic file I/O.
*/

#include "boot.h"

//
//  Reading
//

static int is_delimeter(int c) {
    return isspace(c) || c == EOF || c == '(' || c == ')' || c == '"' || c == ';';
}

static int is_symbol_char(int c) {
    return isalpha(c) || c == '*' || c == '/' ||
           c == '<' || c == '>' || c == '=' || c == '?' || c == '!';
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

static minim_object *read_pair(FILE *in) {
    minim_object *car, *cdr;
    int c;

    skip_whitespace(in);
    c = getc(in);
    if (c == ')') {
        // empty list
        return minim_null;
    }

    ungetc(c, in);
    car = read_object(in);

    skip_whitespace(in);
    c = fgetc(in);
    if (c == '.') {
        // improper list
        peek_expected_delimeter(in);
        cdr = read_object(in);
        skip_whitespace(in);

        c = getc(in);
        if (c != ')') {
            fprintf(stderr, "missing ')' to terminate pair");
            exit(1);
        }

        return make_pair(car, cdr);
    } else {
        // list
        ungetc(c, in);
        cdr = read_pair(in);
        return make_pair(car, cdr);
    }
}

minim_object *read_object(FILE *in) {
    char buffer[SYMBOL_MAX_LEN];
    long num;
    int i;
    short sign;
    char c;
    
    skip_whitespace(in);
    c = getc(in);

    if (c == '#') {
        // special value
        c = getc(in);
        switch (c) {
        case 't':
            return minim_true;
        case 'f':
            return minim_false;
        case '\\':
            return read_char(in);
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

        // compose and check for delimeter
        num *= sign;
        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            exit(1);
        }

        ungetc(c, in);
        return make_fixnum(num);
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
        return intern_symbol(symbols, buffer);
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
    } else if (c == '(') {
        // empty list or pair
        return read_pair(in);
    } else if (c == '\'') {
        // quoted expression
        return make_pair(intern_symbol(symbols, "quote"), make_pair(read_object(in), minim_null));
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

static void write_pair(FILE *out, minim_pair_object *p) {
    write_object(out, minim_car(p));
    if (minim_is_pair(minim_cdr(p))) {
        fputc(' ', out);
        write_pair(out, ((minim_pair_object *) minim_cdr(p)));
    } else if (minim_is_null(minim_cdr(p))) {
        return;
    } else {
        fprintf(out, " . ");
        write_object(out, minim_cdr(p));
    }
}

void write_object(FILE *out, minim_object *o) {
    switch (o->type) {
    case MINIM_NULL_TYPE:
        fprintf(out, "'()");
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
        fprintf(out, "'%s", minim_symbol(o));
        break;
    case MINIM_FIXNUM_TYPE:
        fprintf(out, "%ld", minim_fixnum(o));
        break;
    case MINIM_CHAR_TYPE:
        int c = minim_char(o);
        switch (c) {
        case '\n':
            fprintf(out, "#\\newline");
            break;
        case ' ':
            fprintf(out, "#\\space");
            break;
        default:
            fprintf(out, "#\\%c", c);
        }
        break;
    case MINIM_STRING_TYPE:
        char *str = minim_string(o);
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
        break;
    case MINIM_PAIR_TYPE:
        fputs("'(", out);
        write_pair(out, ((minim_pair_object *) o));
        fputc(')', out);
        break;
    case MINIM_PRIM_PROC_TYPE:
    case MINIM_CLOSURE_PROC_TYPE:
        fprintf(out, "#<procedure>");
        break;
    case MINIM_OUTPUT_PORT_TYPE:
        fprintf(out, "#<output-port>");
        break;
    case MINIM_INPUT_PORT_TYPE:
        fprintf(out, "#<input-port>");
        break;
    
    default:
        fprintf(stderr, "cannot write unknown object");
        exit(1);
    }
}

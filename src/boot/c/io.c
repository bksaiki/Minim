/*
    Basic I/O
*/

#include "../minim.h"

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

static minim_object *read_vector(FILE *in) {
    minim_object *v, *vn, *e;
    int c;

    v = minim_empty_vec;
    while (1) {
        skip_whitespace(in);
        c = peek_char(in);
        if (c == ')') {
            read_char(in);
            break;
        }

        e = read_object(in);
        if (v == minim_empty_vec) {
            v = make_vector(1, e);
        } else {
            vn = make_vector(minim_vector_len(v) + 1, NULL);
            memcpy(minim_vector_arr(vn), minim_vector_arr(v), minim_vector_len(v) * sizeof(minim_vector_object*));
            minim_vector_ref(vn, minim_vector_len(v)) = e;
            v = vn;
        }
    }

    return v;
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
        case '(':
            // vector
            return read_vector(in);
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
                } else if (c == '\'') {
                    c = '\'';
                } else if (c == '\"') {
                    c = '\"';
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
    case MINIM_VECTOR_TYPE:
        if (!quote) fputc('\'', out);
        if (minim_vector_len(o) == 0) {
            fputs("#()", out);
        } else {
            fputs("#(", out);
            write_object2(out, minim_vector_ref(o, 0), 1, display);
            for (long i = 1; i < minim_vector_len(o); ++i) {
                fputc(' ', out);
                write_object2(out, minim_vector_ref(o, i), 1, display);
            }
            fputc(')', out);
        }
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
    case MINIM_PATTERN_VAR_TYPE:
        fprintf(out, "#<pattern>");
        break;
    case MINIM_VALUES_TYPE:
        fprintf(stderr, "cannot write multiple values\n");
        exit(1);

    default:
        fprintf(stderr, "cannot write unknown object\n");
        exit(1);
    }
}

void write_object(FILE *out, minim_object *o) {
    write_object2(out, o, 0, 0);
}

//
//  Objects
//

static void gc_port_dtor(void *ptr) {
    minim_port_object *o = ((minim_port_object *) ptr);
    if (minim_port_is_open(o))
        fclose(minim_port(o));
}

minim_object *make_input_port(FILE *stream) {
    minim_port_object *o = GC_alloc_opt(sizeof(minim_port_object), gc_port_dtor, NULL);
    o->type = MINIM_PORT_TYPE;
    o->flags = MINIM_PORT_READ_ONLY;
    o->stream = stream;
    return ((minim_object *) o);
}

minim_object *make_output_port(FILE *stream) {
    minim_port_object *o = GC_alloc_opt(sizeof(minim_port_object), gc_port_dtor, NULL);
    o->type = MINIM_PORT_TYPE;
    o->flags = 0x0;
    o->stream = stream;
    return ((minim_object *) o);
}

//
//  Primitives
//

minim_object *is_input_port_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_input_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_output_port_proc(minim_object *args) {
    // (-> any boolean)
    return minim_is_output_port(minim_car(args)) ? minim_true : minim_false;
}

minim_object *current_input_port_proc(minim_object *args) {
    // (-> input-port)
    return input_port(current_thread());
}

minim_object *current_output_port_proc(minim_object *args) {
    // (-> output-port)
    return output_port(current_thread());
}

minim_object *open_input_port_proc(minim_object *args) {
    // (-> string input-port)
    minim_object *port;
    FILE *stream;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("open-input-port", "string?", minim_car(args));

    stream = fopen(minim_string(minim_car(args)), "r");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    port = make_input_port(stream);
    minim_port_set(port, MINIM_PORT_OPEN);
    return port;
}

minim_object *open_output_port_proc(minim_object *args) {
    // (-> string output-port)
    minim_object *port;
    FILE *stream;

    if (!minim_is_string(minim_car(args)))
        bad_type_exn("open-output-port", "string?", minim_car(args));
    
    stream = fopen(minim_string(minim_car(args)), "w");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(minim_car(args)));
        exit(1);
    }

    port = make_output_port(stream);
    minim_port_set(port, MINIM_PORT_OPEN);
    return port;
}

minim_object *close_input_port_proc(minim_object *args) {
    // (-> input-port)
    if (!minim_is_input_port(minim_car(args)))
        bad_type_exn("close-input-port", "input-port?", minim_car(args));

    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *close_output_port_proc(minim_object *args) {
    // (-> output-port)
    if (!minim_is_output_port(minim_car(args)))
        bad_type_exn("close-output-port", "output-port?", minim_car(args));

    fclose(minim_port(minim_car(args)));
    minim_port_unset(minim_car(args), MINIM_PORT_OPEN);
    return minim_void;
}

minim_object *read_proc(minim_object *args) {
    // (-> any)
    // (-> input-port any)
    minim_object *in_p, *o;

    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("read", "input-port?", in_p);
    }

    o = read_object(minim_port(in_p));
    return (o == NULL) ? minim_eof : o;
}

minim_object *read_char_proc(minim_object *args) {
    // (-> char)
    // (-> input-port char)
    minim_object *in_p;
    int ch;
    
    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("read-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *peek_char_proc(minim_object *args) {
    // (-> char)
    // (-> input-port char)
    minim_object *in_p;
    int ch;
    
    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_eof : make_char(ch);
}

minim_object *char_is_ready_proc(minim_object *args) {
    // (-> boolean)
    // (-> input-port boolean)
    minim_object *in_p;
    int ch;

    if (minim_is_null(args)) {
        in_p = input_port(current_thread());
    } else {
        in_p = minim_car(args);
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_false : minim_true;
}

minim_object *display_proc(minim_object *args) {
    // (-> any void)
    // (-> any output-port void)
    minim_object *out_p, *o;

    o = minim_car(args);
    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 0, 1);
    return minim_void;
}

minim_object *write_proc(minim_object *args) {
    // (-> any void)
    // (-> any output-port void)
    minim_object *out_p, *o;

    o = minim_car(args);
    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object(minim_port(out_p), o);
    return minim_void;
}

minim_object *write_char_proc(minim_object *args) {
    // (-> char void)
    // (-> char output-port void)
    minim_object *out_p, *ch;

    ch = minim_car(args);
    if (!minim_is_char(ch))
        bad_type_exn("write-char", "char?", ch);

    if (minim_is_null(minim_cdr(args))) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_cadr(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    putc(minim_char(ch), minim_port(out_p));
    return minim_void;
}

minim_object *newline_proc(minim_object *args) {
    // (-> void)
    // (-> output-port void)
    minim_object *out_p;

    if (minim_is_null(args)) {
        out_p = output_port(current_thread());
    } else {
        out_p = minim_car(args);
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    putc('\n', minim_port(out_p));
    return minim_void;
}

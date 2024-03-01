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

static void read_expected_string(FILE *in, const char *s) {
    int c;

    while (*s != '\0') {
        c = getc(in);
        if (c != *s) {
            fprintf(stderr, "unexpected character: '%c'\n", c);
            minim_shutdown(1);
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

static mobj *read_char(FILE *in) {
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
            return Mchar(' ');
        }
        break;
    case 'n':
        if (peek_char(in) == 'e') {
            read_expected_string(in, "ewline");
            peek_expected_delimeter(in);
            return Mchar('\n');
        }
        break;
    default:
        break;
    }

    peek_expected_delimeter(in);
    return Mchar(c);
}

static mobj *read_pair(FILE *in, char open_paren) {
    mobj *car, *cdr;
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

static mobj *read_vector(FILE *in) {
    mobj *l;
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

mobj *read_object(FILE *in) {
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

//
//  Writing
//

static void write_pair(FILE *out, mobj p, int quote, int display) {
    write_object2(out, minim_car(p), quote, display);
    if (minim_consp(minim_cdr(p))) {
        fputc(' ', out);
        write_pair(out, minim_cdr(p), quote, display);
    } else if (minim_nullp(minim_cdr(p))) {
        return;
    } else {
        fprintf(out, " . ");
        write_object2(out, minim_cdr(p), quote, display);
    }
}

void write_object2(FILE *out, mobj *o, int quote, int display) {
    mobj *it;
    minim_thread *th;
    char *str;
    long stashc, i;

    if (minim_nullp(o)) {
        if (!quote) fputc('\'', out);
        fprintf(out, "()");
    } else if (minim_truep(o)) {
        fprintf(out, "#t");
    } else if (minim_falsep(o)) {
        fprintf(out, "#f");
    } else if (minim_eofp(o)) {
        fprintf(out, "#<eof>");
    } else if (minim_voidp(o)) {
        fprintf(out, "#<void>");
    } else if (minim_empty_vecp(o)) {
        if (!quote) fputc('\'', out);
        fprintf(out, "#()");
    } else if (minim_base_rtdp(o)) {
        fprintf(out, "#<base-record-type>");
    } else {
        switch (minim_type(o)) {
        case MINIM_OBJ_CHAR:
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
        case MINIM_OBJ_FIXNUM:
            fprintf(out, "%ld", minim_fixnum(o));
            break;
        case MINIM_OBJ_SYMBOL:
            if (!quote) fputc('\'', out);
            fprintf(out, "%s", minim_symbol(o));
            break;
        case MINIM_OBJ_STRING:
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
        case MINIM_OBJ_PAIR:
            if (!quote) fputc('\'', out);
            fputc('(', out);
            write_pair(out, o, 1, display);
            fputc(')', out);
            break;
        case MINIM_OBJ_VECTOR:
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
        case MINIM_OBJ_BOX:
            if (!quote) fputc('\'', out);
            fputs("#&", out);
            write_object2(out, minim_unbox(o), 1, display);
            break;
        case MINIM_OBJ_PRIM:
            fprintf(out, "#<procedure:%s>", minim_prim_name(o));
            break;
        case MINIM_OBJ_CLOSURE:
            if (minim_closure_name(o) != NULL)
                fprintf(out, "#<procedure:%s>", minim_closure_name(o));
            else
                fprintf(out, "#<procedure>");
            break;
        case MINIM_OBJ_NATIVE_CLOSURE:
            if (minim_native_closure_name(o) != NULL)
                fprintf(out, "#<procedure:%s>", minim_native_closure_name(o));
            else
                fprintf(out, "#<procedure>");
            break;
        case MINIM_OBJ_PORT:
            if (minim_port_readp(o))
                fprintf(out, "#<input-port>");
            else
                fprintf(out, "#<output-port>");
            break;
        case MINIM_OBJ_HASHTABLE:
            if (minim_hashtable_count(o) == 0) {
                fprintf(out, "#<hashtable>");
            } else {
                fprintf(out, "#<hashtable");
                for (i = 0; i < minim_hashtable_alloc(o); ++i) {
                    it = minim_hashtable_bucket(o, i);
                    if (it) {
                        for (; !minim_nullp(it); it = minim_cdr(it)) {
                            fputs(" (", out);
                            write_object2(out, minim_caar(it), 1, display);
                            fputs(" . ", out);
                            write_object2(out, minim_cdar(it), 1, display);
                            fputc(')', out);
                        }
                    }
                }
                fputc('>', out);
            }
            break;
        case MINIM_OBJ_RECORD:
            if (minim_record_rtd(o) == minim_base_rtd) {
                // record type descriptor
                fprintf(out, "#<record-type:%s>", minim_symbol(record_rtd_name(o)));
            } else {
                // record value
                th = current_thread();
                stashc = stash_call_args();

                push_call_arg(o);
                push_call_arg(Moutput_port(out));   // TODO: problematic
                push_call_arg(Mcons(Mfixnum(quote), Mfixnum(display)));
                call_with_args(record_write_proc(th), global_env(th));

                prepare_call_args(stashc);
            }
            break;
        case MINIM_OBJ_SYNTAX:
            fprintf(out, "#<syntax:");
            write_object2(out, strip_syntax(o), 1, display);
            fputc('>', out);
            break;
        case MINIM_OBJ_PATTERN:
            fprintf(out, "#<pattern>");
            break;
        case MINIM_OBJ_ENV:
            fprintf(out, "#<environment>");
            break;
        default:
            fprintf(out, "#<garbage>");
            break;
        }
    }
}

void write_object(FILE *out, mobj *o) {
    write_object2(out, o, 0, 0);
}

//
//  fprintf
//

void minim_fprintf(FILE *o, const char *form, int v_count, mobj **vs, const char *prim_name) {
    long i;
    int vi, fi;

    // verify arity
    fi = 0;
    for (i = 0; form[i]; ++i) {
        if (form[i] == '~') {
            i++;
            if (!form[i]) {
                fprintf(stderr, "%s: ill-formed formatting escape\n", prim_name);
                fprintf(stderr, " at: ");
                write_object2(stderr, Mstring(form), 1, 0);
                fprintf(stderr, "\n");
                exit(1);
            }

            switch (form[i])
            {
            case 'a':
                // display
                fi++;
                break;
            case 's':
                // write
                fi++;
                break;
            default:
                break;
            }
        }
    }

    if (fi != v_count) {
        fprintf(stderr, "%s: format string requires %d arguments, given %d\n", prim_name, fi, v_count);
        fprintf(stderr, " at: ");
        write_object2(stderr, Mstring(form), 1, 0);
        fprintf(stderr, "\n");
        exit(1);
    }

    // try printing
    vi = 0;
    for (i = 0; form[i]; ++i) {
        if (form[i] == '~') {
            // escape character expected
            i++;
            switch (form[i])
            {
            case '~':
                // tilde
                putc('~', o);
                break;
            case 'a':
                // display
                write_object2(o, vs[vi++], 1, 1);
                break;
            case 's':
                // write
                write_object2(o, vs[vi++], 1, 0);
                break;
            default:
                fprintf(stderr, "%s: unknown formatting escape\n", prim_name);
                fprintf(stderr, " at: %s\n", form);
                exit(1);
                break;
            }
        } else {
            putc(form[i], o);
        }
    }
}

//
//  Objects
//

static void gc_port_dtor(void *ptr, void *data) {
    mobj o = ((mobj) ptr);
    if (minim_port_openp(o) &&
        minim_port(o) != stdout &&
        minim_port(o) != stderr &&
        minim_port(o) != stdin)
        fclose(minim_port(o));
}

mobj Minput_port(FILE *stream) {
    mobj o = GC_alloc(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = PORT_FLAG_READ;
    minim_port(o) = stream;
    GC_register_dtor(o, gc_port_dtor);
    return o;
}

mobj Moutput_port(FILE *stream) {
    mobj o = GC_alloc(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = 0x0;
    minim_port(o) = stream;
    GC_register_dtor(o, gc_port_dtor);
    return o;
}

//
//  Primitives
//

mobj *is_input_port_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_input_portp(args[0]) ? minim_true : minim_false;
}

mobj *is_output_port_proc(int argc, mobj **args) {
    // (-> any boolean)
    return minim_output_portp(args[0]) ? minim_true : minim_false;
}

mobj *current_input_port_proc(int argc, mobj **args) {
    // (-> input-port)
    return input_port(current_thread());
}

mobj *current_output_port_proc(int argc, mobj **args) {
    // (-> output-port)
    return output_port(current_thread());
}

mobj *open_input_port_proc(int argc, mobj **args) {
    // (-> string input-port)
    mobj *port;
    FILE *stream;

    if (!minim_stringp(args[0]))
        bad_type_exn("open-input-port", "string?", args[0]);

    stream = fopen(minim_string(args[0]), "r");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(args[0]));
        minim_shutdown(1);
    }

    port = Minput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN);
    return port;
}

mobj *open_output_port_proc(int argc, mobj **args) {
    // (-> string output-port)
    mobj *port;
    FILE *stream;

    if (!minim_stringp(args[0]))
        bad_type_exn("open-output-port", "string?", args[0]);
    
    stream = fopen(minim_string(args[0]), "w");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(args[0]));
        minim_shutdown(1);
    }

    port = Moutput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN);
    return port;
}

mobj *close_input_port_proc(int argc, mobj **args) {
    // (-> input-port)
    if (!minim_input_portp(args[0]))
        bad_type_exn("close-input-port", "input-port?", args[0]);

    fclose(minim_port(args[0]));
    minim_port_unset(args[0], PORT_FLAG_OPEN);
    return minim_void;
}

mobj *close_output_port_proc(int argc, mobj **args) {
    // (-> output-port)
    if (!minim_is_output_port(args[0]))
        bad_type_exn("close-output-port", "output-port?", args[0]);

    fclose(minim_port(args[0]));
    minim_port_unset(args[0], PORT_FLAG_OPEN);
    return minim_void;
}

mobj *read_proc(int argc, mobj **args) {
    // (-> any)
    // (-> input-port any)
    mobj *in_p, *o;

    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_is_input_port(in_p))
            bad_type_exn("read", "input-port?", in_p);
    }

    o = read_object(minim_port(in_p));
    return (o == NULL) ? minim_eof : o;
}

mobj *read_char_proc(int argc, mobj **args) {
    // (-> char)
    // (-> input-port char)
    mobj *in_p;
    int ch;
    
    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_is_input_port(in_p))
            bad_type_exn("read-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj *peek_char_proc(int argc, mobj **args) {
    // (-> char)
    // (-> input-port char)
    mobj *in_p;
    int ch;
    
    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj *char_is_ready_proc(int argc, mobj **args) {
    // (-> boolean)
    // (-> input-port boolean)
    mobj *in_p;
    int ch;

    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_is_input_port(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_false : minim_true;
}

mobj *display_proc(int argc, mobj **args) {
    // (-> any void)
    // (-> any output-port void)
    mobj *out_p, *o;

    o = args[0];
    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_is_output_port(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 0, 1);
    return minim_void;
}

mobj *write_proc(int argc, mobj **args) {
    // (-> any void)
    // (-> any output-port void)
    mobj *out_p, *o;

    o = args[0];
    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_is_output_port(out_p))
            bad_type_exn("write", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 1, 0);
    return minim_void;
}

mobj *write_char_proc(int argc, mobj **args) {
    // (-> char void)
    // (-> char output-port void)
    mobj *out_p, *ch;

    ch = args[0];
    if (!minim_charp(ch))
        bad_type_exn("write-char", "char?", ch);

    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_is_output_port(out_p))
            bad_type_exn("write-char", "output-port?", out_p);
    }

    putc(minim_char(ch), minim_port(out_p));
    return minim_void;
}

mobj *newline_proc(int argc, mobj **args) {
    // (-> void)
    // (-> output-port void)
    mobj *out_p;

    if (argc == 0) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[0];
        if (!minim_is_output_port(out_p))
            bad_type_exn("newline", "output-port?", out_p);
    }

    putc('\n', minim_port(out_p));
    return minim_void;
}

mobj *fprintf_proc(int argc, mobj **args) {
    // (-> output-port string any ... void)
    if (!minim_is_output_port(args[0]))
        bad_type_exn("fprintf", "output-port?", args[0]);
    
    if (!minim_stringp(args[1]))
        bad_type_exn("fprintf", "string?", args[1]);
    
    minim_fprintf(minim_port(args[0]), minim_string(args[1]), argc - 2, &args[2], "fprintf");
    return minim_void;
}

mobj *printf_proc(int argc, mobj **args) {
    // (-> string any ... void)
    mobj *curr_op;

    if (!minim_stringp(args[0]))
        bad_type_exn("printf", "string?", args[0]);

    curr_op = output_port(current_thread());
    minim_fprintf(minim_port(curr_op), minim_string(args[0]), argc - 1, &args[1], "printf");
    return minim_void;
}
